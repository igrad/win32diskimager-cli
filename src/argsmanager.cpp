#include "argsmanager.h"

namespace {
const QMap<ArgID, QString> ALL_ARGS_MAP = {
   { ArgID::Headless, "headless" },
   { ArgID::Image, "image" },
   { ArgID::Volume, "volume" },
   { ArgID::Drive, "drive" },
   { ArgID::SkipConfirmation, "skip-confirmation" },
   { ArgID::Hash, "hash" },
   { ArgID::WriteHashToFile, "write-hash-to-file" },
   { ArgID::Write, "write" },
   { ArgID::Read, "read" },
   { ArgID::ReadOnlyAllocatedPartitions, "read-only-allocated-partitions" },
   { ArgID::VerifyOnly, "verify-only" },
   { ArgID::WriteOut, "write-out" },
   { ArgID::Quiet, "quiet" },
   { ArgID::Verbose, "verbose" }
};

QMap<ArgID, Arg> BuildAllArgData()
{
   QMap<ArgID, Arg> data;

   Arg Headless = {
                   'h',
      "headless",
      "Run without GUI. This is implied if 3 or more arguments are passed in."
   };

   Arg Image = {
                'i',
      "Image",
      "The image file to read into/write from."
   };

   Arg Volume = {
                 'v',
      "volume",
      "The volume to read from/write to."
   };

   Arg Drive = {
                'd',
      "drive",
      "Same as -v. If both -v and -d are specified, -v is ignored."
   };

   Arg SkipConfirmation = {
                           's',
      "skip-confirmation",
      "Skip all confirmation prompts."
   };

   Arg Hash = {
               'x',
      "hash",
      "Hash algorthm to use. If -f no hash algorithm is specified, SHA256 will be used. Options are MD5, SHA1, and SHA256."
   };

   Arg WriteHashToFile = {
                          'f',
      "write-hash-to-file",
      "The generated has will be written to this file."
   };

   Arg Write = {
                'w',
      "write",
      "Write image file to volume."
   };

   Arg Read = {
               'r',
      "read",
      "Read image from volume to file."
   };

   Arg ReadOnlyAllocatedPartitions = {
                                      'a',
      "read-only-allocated-partitions",
      "Read only from allocated partiftions on the volume."
   };

   Arg VerifyOnly = {
                     'V',
      "verify-only",
      "Only verify the image."
   };

   Arg WriteOut = {
                   'o',
      "write-out",
      "Write all output from this command run to a specified file. If -q is not specified, output will still print to the shell."
   };

   Arg Quiet = {
                'q',
      "quiet",
      "Hush all output from this command. If -o is specified in tandem, output will only be put to the output file."
   };

   Arg Verbose = {
                  'v',
      "verbose",
      "Verbose output (mostly for debugging)"
   };

   Arg Help = {
      '\n',
      "help",
      "Display this help dialog."
   };

   data[ArgID::Headless] = Headless;
   data[ArgID::Image] = Image;
   data[ArgID::Volume] = Volume;
   data[ArgID::Drive] = Drive;
   data[ArgID::SkipConfirmation] = SkipConfirmation;
   data[ArgID::Hash] = Hash;
   data[ArgID::WriteHashToFile] = WriteHashToFile;
   data[ArgID::Write] = Write;
   data[ArgID::Read] = Read;
   data[ArgID::ReadOnlyAllocatedPartitions] = ReadOnlyAllocatedPartitions;
   data[ArgID::VerifyOnly] = VerifyOnly;
   data[ArgID::WriteOut] = WriteOut;
   data[ArgID::Quiet] = Quiet;
   data[ArgID::Verbose] = Verbose;
   data[ArgID::Help] = Help;

   return data;
}
}

ArgsManager::AllArgData = BuildAllArgData();

ArgsManager::ArgsManager(int argc, char* argv[])
{
   ParseArgs(argc, argv);
}

const Arg ArgsManager::GetArg(const ArgID id) const
{
   return ParsedArgs.value(id);
}

void ArgsManager::ParseArgs(int argc, char* argv[])
{
   cxxopts::Options options("win32diskimager",
                            "A Windows tool for writing images to USB sticks or SD/CF cards.");

   for(const auto& pair : std::as_const(AllArgData).asKeyValueRange())
   {
      const ArgID id = pair.first;
      const Arg arg = pair.second;

      options.add_options()
         (arg.Long.insert(0, arg.Short).toStdString.c_str(),
          arg.Description.toStdString.c_str());
   }

   // options.add_options()
   //    ("h,headless", "Run without GUI. This is implied if 3 or more arguments are passed in.")
   //    ("i,image", "The image file to read into/write from")
   //    ("v,volume", "The volume to read from/write to")
   //    ("d,drive", "Same as -v.")
   //    ("s,skip-confirmation", "Skip all confirmation prompts")
   //    ("x,hash", "Hash algorthm to use. If -f no hash algorithm is specified, SHA256 will be used. Options are MD5, SHA1, and SHA256.")
   //    ("f,write-hash-to-file", "The generated has will be written to this file")
   //    ("w,write", "Write image file to volume")
   //    ("r,read", "Read image from volume to file")
   //    ("a,read-only-allocated-partitions", "Read only from allocated partiftions on the volume")
   //    ("V,verify-only", "Only verify the image")
   //    ("o,write-out", "Write all output from this command run to a specified file. If -q is not specified, output will still print to the shell.")
   //    ("q,quiet", "Hush all output from this command. If -o is specified in tandem, output will only be put to the output file.")
   //    ("verbose", "Verbose output (mostly for debugging)", cxxopts::value<bool>()->default_value("false"))
   //    s

   auto args = options.parse(argc, argv);

   for(const auto& pair : std::as_const(AllArgData).asKeyValueRange())
   {
      const ArgID id = pair.first;
      const std::string argLong = pair.second.Long.asStdString();
      if(args.count(str) > 0)
      {
         ParsedArgs[id] = QVariant(QString::fromStdString(args[argLong].as<std::string>()));
      }
   }
}
