#include "argsmanager.h"

ArgsManager::ArgsManager(int argc, char* argv[])
{
    ParseArgs(argc, argv);
}

const Arg& ArgsManager::GetArg(const ArgID id) const
{
    return ParsedArgs[id];
}

void ArgsManager::ParseArgs(const int argc, const char* argv[])
{
   cxxopts::Options options("win32diskimager",
      "A Windows tool for writing images to USB sticks or SD/CF cards.");

   options.add_options()
      ("h,headless", "Run without GUI. This is implied if 3 or more arguments are passed in.")
      ("i,image", "The image file to read into/write from")
      ("v,volume", "The volume to read from/write to")
      ("d,drive", "Same as -v")
      ("s,skip-confirmation", "Skip all confirmation prompts")
      ("x,hash", "Hash algorthm to use. If -f no hash algorithm is specified, SHA256 will be used. Options are MD5, SHA1, and SHA256.")
      ("f,write-hash-to-file", "The generated has will be written to this file")
      ("w,write", "Write image file to volume")
      ("r,read", "Read image from volume to file")
      ("a,read-only-allocated-partitions", "Read only from allocated partiftions on the volume")
      ("V,verify-only", "Only verify the image")
      ("o,write-out", "Write all output from this command run to a specified file. If -q is not specified, output will still print to the shell.")
      ("q,quiet", "Hush all output from this command. If -o is specified in tandem, output will only be put to the output file.")
      ("verbose", "Verbose output (mostly for debugging)", cxxopts::value<bool>()->default_value("false"));

   auto args = options.parse(argc, argv);
   const bool headlessMode = argc > 2;
}
