from argparse import ArgumentParser
import msscmp, os

def main():
    argparser = ArgumentParser(description="MSS Soundbank tool for parsing and converting .msscmp files and .binka audio.")
    subparsers = argparser.add_subparsers(dest="command")

    bank_parser = subparsers.add_parser("bank", help="Parse and dump sources from a .msscmp soundbank.")
    bank_parser.add_argument("filepath", help="Path to the .msscmp soundbank file.")
    bank_parser.add_argument("-v", "--verbose", action="store_true", default=False, help="Output debug info.")
    bank_parser.add_argument("-d", "--dump", default=None, metavar="path", help="Dump bank sources into a directory.")

    convert_parser = subparsers.add_parser("convert", help="Convert .binka file(s) to .ogg.")
    convert_parser.add_argument("path", help="Path to a .binka file or a directory containing .binka files.")
    convert_parser.add_argument("--remove", action="store_true", default=False, help="Remove original .binka files after conversion.")

    args = argparser.parse_args()

    if args.command == "bank":
        if not args.filepath.endswith(".msscmp"):
            raise Exception("Not a Soundbank(.msscmp) file")
        with open(args.filepath, "rb") as file:
            msscmpfile = msscmp.MsscmpParser(args.verbose)
            msscmpfile.process(file)
            if args.dump is not None:
                print("Dumping...")
                msscmpfile.dumpAllSources(args.dump)

    elif args.command == "convert":
        if os.path.isdir(args.path):
            msscmp.convertBinkaDirectory(args.path, args.remove)
        elif os.path.isfile(args.path) and args.path.endswith(".binka"):
            msscmp.convertBinka(args.path, args.remove)
        else:
            raise Exception("Path must be a .binka file or a directory.")

    else:
        argparser.print_help()

if __name__ == "__main__":
    main()