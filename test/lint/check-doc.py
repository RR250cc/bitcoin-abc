#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Bitcoin Core developers
# Copyright (c) 2019 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

'''
This checks if all command line args are documented.
Return value is 0 to indicate no error.

Author: @MarcoFalke
'''

import glob
import re
from pprint import PrettyPrinter
from subprocess import check_output

TOP_LEVEL = 'git rev-parse --show-toplevel'
FOLDER_SRC = '/src/**/'
FOLDER_TEST = '/src/**/test/'

EXTENSIONS = ["*.c", "*.h", "*.cpp", "*.cc", "*.hpp"]
REGEX_ARG = r'(?:ForceSet|SoftSet|Get|Is)(?:Bool|Int)?Args?(?:Set)?\(\s*"(-[^"]+)"'
REGEX_DOC = r'AddArg\(\s*"(-[^"=]+?)(?:=|")'

# list false positive unknows arguments
SET_FALSE_POSITIVE_UNKNOWNS = set([
    '-includeconf',
    '-regtest',
    '-testnet',
    '-zmqpubhashblock',
    '-zmqpubhashtx',
    '-zmqpubrawblock',
    '-zmqpubrawtx',
    '-zmqpubhashblockhwm',
    '-zmqpubhashtxhwm',
    '-zmqpubrawblockhwm',
    '-zmqpubrawtxhwm',
    '-zmqpubsequence',
    '-zmqpubsequencehwm',
])

# list false positive undocumented arguments
SET_FALSE_POSITIVE_UNDOCUMENTED = set([
    '-help',
    '-h',
    '-dbcrashratio',
    '-enableminerfund',
    '-forcecompactdb',
    '-maxaddrtosend',
    '-parkdeepreorg',
    '-automaticunparking',
    # Removed arguments that now just print a helpful error message
    '-zapwallettxes',
    '-replayprotectionactivationtime',
    # Remove after May 2023 upgrade
    '-wellingtonactivationtime',
])


def main():
    top_level = check_output(TOP_LEVEL, shell=True,
                             universal_newlines=True, encoding='utf8').strip()
    source_files = []
    test_files = []

    for extension in EXTENSIONS:
        source_files += glob.glob(top_level +
                                  FOLDER_SRC + extension, recursive=True)
        test_files += glob.glob(top_level + FOLDER_TEST +
                                extension, recursive=True)

    files = set(source_files) - set(test_files)

    args_used = set()
    args_docd = set()
    for file in files:
        with open(file, 'r', encoding='utf-8') as f:
            content = f.read()
            args_used |= set(re.findall(re.compile(REGEX_ARG), content))
            args_docd |= set(re.findall(re.compile(REGEX_DOC), content))

    args_used |= SET_FALSE_POSITIVE_UNKNOWNS
    args_docd |= SET_FALSE_POSITIVE_UNDOCUMENTED
    args_need_doc = args_used - args_docd
    args_unknown = args_docd - args_used

    pp = PrettyPrinter()
    print("Args used        : {}".format(len(args_used)))
    print("Args documented  : {}".format(len(args_docd)))
    print("Args undocumented: {}".format(len(args_need_doc)))
    pp.pprint(args_need_doc)
    print("Args unknown     : {}".format(len(args_unknown)))
    pp.pprint(args_unknown)


if __name__ == "__main__":
    main()
