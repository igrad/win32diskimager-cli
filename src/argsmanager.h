#pragma once

#include <QString>
#include <QVariant>
#include <QMap>
#include <cxxopts.hpp>

struct Arg
{
    char Short;
    QString Long;
    QString Description;
    QVariant Value;
};

enum class ArgID: int
{
    Undefined = 0,
    Headless,
    Image,
    Volume,
    Drive,
    SkipConfirmation,
    Hash,
    WriteHashToFile,
    Write,
    Read,
    ReadOnlyAllocatedPartitions,
    VerifyOnly,
    WriteOut,
    Quiet,
    Verbose
};

class ArgsManager
{
public:
    ArgsManager(int argc, char* argv[]);
    ~ArgsManager() = default;

    const Arg& GetArg(const ArgID id) const;

private:
    void ParseArgs(const int argc, const char* argv[]);

    QMap<ArgID, Arg> ParsedArgs;
};
