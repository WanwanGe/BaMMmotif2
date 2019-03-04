#!/usr/bin/env python3
'''
This is a script to convert PWMs from a MEME-formatted file to BaMM-formatted file(s).
Prerequisite: The input file must be in MEME-format (version 4).
'''

import argparse
import os

from utils import parse_meme, write_bamm

def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('meme_file')
    parser.add_argument('-o', default=None)

    return parser

def main():
    parser = create_parser()
    args = parser.parse_args()

    ipath = args.meme_file
    if args.o is None:
        dir = os.path.dirname(ipath)
    else:
        dir = args.o
        if not os.path.exists(dir):
            os.makedirs(dir)
    basename = os.path.splitext(os.path.basename(ipath))[0]
    motifset = parse_meme(ipath)
    models = motifset['models']

    for num in range(len(models)):
        filepath_v = os.path.join(dir, basename + "_motif_" + str(num+1) + ".ihbcp")
        filepath_p = os.path.join(dir, basename + "_motif_" + str(num+1) + ".ihbp")
        write_bamm(models[num]['pwm'], filepath_v )
        write_bamm(models[num]['pwm'], filepath_p )


if __name__ == '__main__':
    main()


