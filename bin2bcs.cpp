//
//  bin2bcs.c
//  PADE
//
//  Created by sun zhuoshi on 4/15/13.
//  Copyright (c) 2013 l4play. All rights reserved.
//

#include <stdio.h>

#include <iostream>
#include <vector>
#include <list>
#include <string>
#include <sstream>

#include "common.h"

#define BLOCK_SIZE 16

using namespace std;

typedef vector<unsigned char> Block;
typedef list<Block> BlockList;

void write_bc_file(const BlockList &blockList, const std::string &file_name) {
    FILE *fp = fopen(file_name.c_str(), "wb");
    if (!fp) {
        abort_("[write_bc_file] File %s cound not be opened for writing", file_name.c_str());
    }
    cout << "Writing " << file_name << " ..." << endl;
    for (BlockList::const_iterator it=blockList.begin(); it!=blockList.end(); ++it) {
        fwrite(&it->front(), 1, it->size(), fp);
    }
    fclose(fp);
}

void process_bin_file(char *file_name) {
    int seq = 0;
	FILE *fp = fopen(file_name, "rb");
	if (!fp) {
		abort_("[read_bin_file] File %s cound not be opened for reading", file_name);
	}
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size % BLOCK_SIZE) {
        abort_("[read_bin_file] File %s bad size", file_name);
    }
    fseek(fp, 0, SEEK_SET);
    Block block;
    block.resize(BLOCK_SIZE);
    BlockList blockList;
    for (int i=0; i<file_size/BLOCK_SIZE; ++i) {
        if (BLOCK_SIZE != fread(&(block.front()), 1, BLOCK_SIZE, fp)) {
            abort_("[read_bin_file] Fild %s read error", file_name);
        }
        if (0 == memcmp(&block.front(), FCC_TEX1, 4)) {
            if (!blockList.empty()) {
                ostringstream bc_file_name;
                bc_file_name << file_name << "-" << seq << ".bc";
                write_bc_file(blockList, bc_file_name.str());
                seq ++;
                blockList.clear();
            }
            blockList.push_back(block);
        }
        else {
            blockList.push_back(block);
        }
    }
    fclose(fp);
    cout << "Done" << endl;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        abort_("Usage: program_name <file_in>");
    }
    process_bin_file(argv[1]);
    return 0;
}
