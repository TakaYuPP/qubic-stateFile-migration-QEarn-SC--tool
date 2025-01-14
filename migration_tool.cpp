#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include "./m256.h"
#include "keyUtils.h"
#include "K12AndKeyUtil.h"

constexpr uint64_t QEARN_MAX_LOCKS = 4194304;
constexpr uint64_t QEARN_MAX_EPOCHS = 4096;
constexpr uint64_t QEARN_MAX_USERS = 131072;

constexpr uint64_t QEARN_EARLY_UNLOCKING_PERCENT_4_7 = 5;
constexpr uint64_t QEARN_BURN_PERCENT_4_7 = 45;
typedef m256i id;

template <typename T, uint64_t L>
struct array
{
private:
    static_assert(L && !(L & (L - 1)),
        "The capacity of the array must be 2^N."
        );

    T _values[L];

public:
    // Return number of elements in array
    static inline constexpr uint64_t capacity()
    {
        return L;
    }

    // Get element of array
    inline const T& get(uint64_t index) const
    {
        return _values[index & (L - 1)];
    }

    // Set element of array
    inline void set(uint64_t index, const T& value)
    {
        _values[index & (L - 1)] = value;
    }

    // Set content of array by copying memory (size must match)
    template <typename AT>
    inline void setMem(const AT& value)
    {
        static_assert(sizeof(_values) == sizeof(value), "This function can only be used if the overall size of both objects match.");
        // This if is resolved at compile time
        if (sizeof(_values) == 32)
        {
            // assignment uses __m256i intrinsic CPU functions which should be very fast
            *((id*)_values) = *((id*)&value);
        }
        else
        {
            // generic copying
            copyMemory(*this, value);
        }
    }

    // Set all elements to passed value
    inline void setAll(const T& value)
    {
        for (uint64_t i = 0; i < L; ++i)
            _values[i] = value;
    }

    // Set elements in range to passed value
    inline void setRange(uint64_t indexBegin, uint64_t indexEnd, const T& value)
    {
        for (uint64_t i = indexBegin; i < indexEnd; ++i)
            _values[i & (L - 1)] = value;
    }

    // Returns true if all elements of the range equal value (and range is valid).
    inline bool rangeEquals(uint64_t indexBegin, uint64_t indexEnd, const T& value) const
    {
        if (indexEnd > L || indexBegin > indexEnd)
            return false;
        for (uint64_t i = indexBegin; i < indexEnd; ++i)
        {
            if (!(_values[i] == value))
                return false;
        }
        return true;
    }
};


// Divide a by b, but return 0 if b is 0 (rounding to lower magnitude in case of integers)
template <typename T1, typename T2>
inline static auto safe_div(T1 a, T2 b) -> decltype(a / b)
{
    return (b == 0) ? 0 : (a / b);
}

struct RoundInfo {

    uint64_t _totalLockedAmount;            // The initial total locked amount in any epoch.  Max Epoch is 65535
    uint64_t _epochBonusAmount;             // The initial bonus amount per an epoch.         Max Epoch is 65535 

};

struct EpochIndexInfo {

    uint32_t startIndex;
    uint32_t endIndex;
};

struct LockInfo {

    uint64_t _lockedAmount;
    id ID;
    uint32_t _lockedEpoch;

};

struct HistoryInfo {

    uint64_t _unlockedAmount;
    uint64_t _rewardedAmount;
    id _unlockedID;

};

struct StatsInfo {

    uint64_t burnedAmount;
    uint64_t boostedAmount;
    uint64_t rewardedAmount;

};

array<RoundInfo, QEARN_MAX_EPOCHS> _initialRoundInfo;
array<RoundInfo, QEARN_MAX_EPOCHS> _currentRoundInfo;
array<EpochIndexInfo, QEARN_MAX_EPOCHS> _epochIndex;
array<LockInfo, QEARN_MAX_LOCKS> locker;
array<HistoryInfo, QEARN_MAX_USERS> earlyUnlocker;
array<HistoryInfo, QEARN_MAX_USERS> fullyUnlocker;

uint32_t _earlyUnlockedCnt;
uint32_t _fullyUnlockedCnt;
array<StatsInfo, QEARN_MAX_EPOCHS> statsInfo;

// Function to read old state from a file
void readOldState(const std::string& filename) {

    std::ifstream infile(filename, std::ios::binary);

    if (!infile) {
        throw std::runtime_error("Failed to open the old state file.");
    }

    infile.read(reinterpret_cast<char*>(&_initialRoundInfo), sizeof(RoundInfo) * _initialRoundInfo.capacity());
    infile.read(reinterpret_cast<char*>(&_currentRoundInfo), sizeof(RoundInfo) * _currentRoundInfo.capacity());
    infile.read(reinterpret_cast<char*>(&_epochIndex), sizeof(EpochIndexInfo) * _epochIndex.capacity());
    infile.read(reinterpret_cast<char*>(&locker), sizeof(LockInfo) * locker.capacity());
    infile.read(reinterpret_cast<char*>(&earlyUnlocker), sizeof(HistoryInfo) * earlyUnlocker.capacity());
    infile.read(reinterpret_cast<char*>(&fullyUnlocker), sizeof(HistoryInfo) * fullyUnlocker.capacity());
    infile.read(reinterpret_cast<char*>(&_earlyUnlockedCnt), sizeof(_earlyUnlockedCnt));
    infile.read(reinterpret_cast<char*>(&_fullyUnlockedCnt), sizeof(_fullyUnlockedCnt));
}

// Function to write new state to a file
void writeNewState(const std::string& filename) {

    std::ofstream outfile(filename, std::ios::binary);
    if (!outfile) {
        throw std::runtime_error("Failed to open the new state file.");
    }

    outfile.write(reinterpret_cast<const char*>(&_initialRoundInfo), sizeof(RoundInfo) * _initialRoundInfo.capacity());
    outfile.write(reinterpret_cast<const char*>(&_currentRoundInfo), sizeof(RoundInfo) * _currentRoundInfo.capacity());
    outfile.write(reinterpret_cast<const char*>(&_epochIndex), sizeof(EpochIndexInfo) * _epochIndex.capacity());
    outfile.write(reinterpret_cast<const char*>(&locker), sizeof(LockInfo) * locker.capacity());
    outfile.write(reinterpret_cast<const char*>(&earlyUnlocker), sizeof(HistoryInfo) * earlyUnlocker.capacity());
    outfile.write(reinterpret_cast<const char*>(&fullyUnlocker), sizeof(HistoryInfo) * fullyUnlocker.capacity());
    outfile.write(reinterpret_cast<const char*>(&_earlyUnlockedCnt), sizeof(_earlyUnlockedCnt));
    outfile.write(reinterpret_cast<const char*>(&_fullyUnlockedCnt), sizeof(_fullyUnlockedCnt));
    outfile.write(reinterpret_cast<const char*>(&statsInfo), sizeof(StatsInfo) * statsInfo.capacity());

    if (!outfile) {
        throw std::runtime_error("Failed to write id to the file.");
    }
    outfile.close();
}

int main() {
    try {
        // File paths
        const std::string oldStateFile = "contract0009.144";
        const std::string newStateFile = "contract0009.144";

        // Read the old state
        readOldState(oldStateFile);

        StatsInfo initializedStats;

        initializedStats.boostedAmount = 0;
        initializedStats.burnedAmount = 0;
        initializedStats.rewardedAmount = 0;

        for(uint32_t i = 0 ; i < QEARN_MAX_EPOCHS; i++)
        {
            statsInfo.set(i, initializedStats);
        }

        /*
            We did not save the metrics data for epoch 138, 139, 140, 141, 142, 143.
            So we need to save the data in here.
        */
        initializedStats.burnedAmount = safe_div((_initialRoundInfo.get(138)._epochBonusAmount - _currentRoundInfo.get(138)._epochBonusAmount), QEARN_BURN_PERCENT_4_7 + QEARN_EARLY_UNLOCKING_PERCENT_4_7) * QEARN_BURN_PERCENT_4_7;
        initializedStats.rewardedAmount = safe_div((_initialRoundInfo.get(138)._epochBonusAmount - _currentRoundInfo.get(138)._epochBonusAmount), QEARN_BURN_PERCENT_4_7 + QEARN_EARLY_UNLOCKING_PERCENT_4_7) * QEARN_EARLY_UNLOCKING_PERCENT_4_7;
        initializedStats.boostedAmount = safe_div((_initialRoundInfo.get(138)._epochBonusAmount - _currentRoundInfo.get(138)._epochBonusAmount), QEARN_BURN_PERCENT_4_7 + QEARN_EARLY_UNLOCKING_PERCENT_4_7) * (100 - QEARN_BURN_PERCENT_4_7 - QEARN_EARLY_UNLOCKING_PERCENT_4_7);

        statsInfo.set(138, initializedStats);

        initializedStats.burnedAmount = 0;
        initializedStats.rewardedAmount = 0;
        initializedStats.boostedAmount = safe_div(safe_div(_currentRoundInfo.get(139)._epochBonusAmount * 10000000ULL, _currentRoundInfo.get(139)._totalLockedAmount) * (_initialRoundInfo.get(139)._totalLockedAmount - _currentRoundInfo.get(139)._totalLockedAmount), 10000000ULL);

        statsInfo.set(139, initializedStats);

        initializedStats.burnedAmount = 0;
        initializedStats.rewardedAmount = 0;
        initializedStats.boostedAmount = safe_div(safe_div(_currentRoundInfo.get(140)._epochBonusAmount * 10000000ULL, _currentRoundInfo.get(140)._totalLockedAmount) * (_initialRoundInfo.get(140)._totalLockedAmount - _currentRoundInfo.get(140)._totalLockedAmount), 10000000ULL);

        statsInfo.set(140, initializedStats);

        initializedStats.burnedAmount = 0;
        initializedStats.rewardedAmount = 0;
        initializedStats.boostedAmount = safe_div(safe_div(_currentRoundInfo.get(141)._epochBonusAmount * 10000000ULL, _currentRoundInfo.get(141)._totalLockedAmount) * (_initialRoundInfo.get(141)._totalLockedAmount - _currentRoundInfo.get(141)._totalLockedAmount), 10000000ULL);

        statsInfo.set(141, initializedStats);

        initializedStats.burnedAmount = 0;
        initializedStats.rewardedAmount = 0;
        initializedStats.boostedAmount = safe_div(safe_div(_currentRoundInfo.get(142)._epochBonusAmount * 10000000ULL, _currentRoundInfo.get(142)._totalLockedAmount) * (_initialRoundInfo.get(142)._totalLockedAmount - _currentRoundInfo.get(142)._totalLockedAmount), 10000000ULL);

        statsInfo.set(142, initializedStats);

        // Write the new state to a file
        writeNewState(newStateFile);

        std::cout << "Migration completed successfully. New state saved to: " << newStateFile << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}