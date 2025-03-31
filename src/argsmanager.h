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
   Verbose,
   Help
};

class ArgsManager
{
public:
   ArgsManager(int argc, char* argv[]);
   ~ArgsManager() = default;

   const QVariant GetArgValue(const ArgID id) const;

   static const QMap<ArgID, Arg> AllArgData;

private:
   void ParseArgs(int argc, char* argv[]);

   QMap<ArgID, QVariant> ParsedArgs;
};
