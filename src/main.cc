#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <main.h>
#include <tufs.h>
#include <boost/algorithm/string.hpp>

using namespace std;

FILE *input_file;
uint64_t partition_start;
uint64_t partition_size;
string volume_label = "";

void print(string msg){
	cout << msg << "\n";
	fflush(stdout);
}

void expect(bool cond, string msg){
	if(!cond){
		cout << "ERROR: " << msg << "\n";
		exit(-1);
	}
}

void writesect(uint64_t lba, void *buffer){
	fseeko64(input_file, lba*512, SEEK_SET);
	uint8_t *char_s = (uint8_t *)buffer;
	for(int i = 0; i < 512; i++){
		fputc(char_s[i], input_file);
	}
}

void readsect(uint64_t lba, void *buffer){
	fseeko64(input_file, lba*512, SEEK_SET);
	uint8_t *char_s = (uint8_t *)buffer;
	for(int i = 0; i < 512; i++){
		char_s[i] = fgetc(input_file);
	}
}

uint64_t filename_hash(const char *filename){
	uint64_t ret = 0;
	uint64_t highorder = 0;
	for(uint64_t i = 0; i < strlen(filename); i++){
		highorder = ret & 0xf8000000;
		ret <<= 5;
		ret ^= (highorder >> 27);
		ret ^= (uint64_t)filename[i];
	}
	return ret;
}

TUFS1HEADER header;


uint64_t get_path_lba(string path){
	uint64_t ret;
	bool failed = false;
	vector<string> segments;
	boost::split(segments, path, boost::is_any_of("/"));
	TUFS1FILEALLOC directory_entry;
	uint64_t current_lba = header.RDSect;
	readsect(current_lba, &directory_entry);
	if(segments[1] != ""){
		for(int seg = 1; seg < segments.size(); seg++){
			uint64_t segnamehash = filename_hash(segments[seg].c_str());
			uint64_t entrycount = directory_entry.FSIBytes;
			current_lba = directory_entry.FSSect;
			readsect(current_lba, &directory_entry);
			uint64_t entry = 0;
			while(directory_entry.FNHash != segnamehash){
				if(directory_entry.NFIDSect == 0){
					cout << "ERROR: Invalid path.\n";
					failed = true;
					break;
				}
				current_lba = directory_entry.NFIDSect;
				readsect(current_lba, &directory_entry);
				entry++;
			}
			if(failed){
				current_lba = 0;
				break;
			}
		}
	}
	ret = current_lba;
	return ret;
}

vector<string> listdir(string dir){
	uint64_t dir_lba = get_path_lba(dir);
	if(dir_lba == 0){
		return vector<string>();
	}
	TUFS1FILEALLOC dir_alloc;
	readsect(dir_lba, &dir_alloc);
	string filename = "";
	vector<string> ret;
	if(dir_alloc.FSIBytes != 0){
		uint64_t file_lba = dir_alloc.FSSect;
		TUFS1FILEALLOC file_alloc;
		readsect(file_lba, &file_alloc);
		filename = string((char *)file_alloc.FName);
		if((file_alloc.FAttributes & attributes::directory) > 0){
			filename += "/";
		}
		ret.push_back(filename);
		for(uint64_t entry = 1; entry < dir_alloc.FSIBytes; entry++){
			file_lba = file_alloc.NFIDSect;
			readsect(file_lba, &file_alloc);
			filename = string((char *)file_alloc.FName);
			if((file_alloc.FAttributes & attributes::directory) > 0){
				filename += "/";
			}
			ret.push_back(filename);
		}
	}
	return ret;
}

string get_actual_path(string path){
	string ret = "/";
	uint64_t path_lba = get_path_lba(path);
	if(path_lba == 0){
		return "";
	}
	vector<string> actual_path;
	while(1){
		TUFS1FILEALLOC tmp;
		readsect(path_lba, &tmp);
		readsect(tmp.FSSect, &tmp);
		readsect(tmp.PSect, &tmp);
		actual_path.push_back(string((char *)tmp.FName));
		if(path_lba == header.RDSect){
			break;
		}
		path_lba = tmp.PSect;
	}
	reverse(actual_path.begin(), actual_path.end());
	for(string revp : actual_path){
		if(revp != ""){
			if(ret == "/"){
				ret = "";
			}
			ret += "/" + revp;
		}
	}
	return ret;
}

int main(int argc, char **argv){
	expect(argc > 2, "Storage file and or start lba not specified.");
	input_file = fopen(argv[1], "rb+");
	expect(input_file != (FILE *)0, "Storage file invalid.");
	partition_start = stoull(string(argv[2]));
	readsect(partition_start + 1, &header);
	for(uint8_t chr : header.PName){
		volume_label += (char)chr;
	}
	partition_size = header.PSISect;
	string current_path = "/";
	expect(header.VNumber >= 1, "Partition file system is of an incompatible version.");
	while(1){
		cout << "root@" << volume_label << ":" << current_path << "$ ";
		string inputed;
		getline(cin, inputed);
		vector<string> toks;
		boost::split(toks, inputed, boost::is_any_of(" "));
		if(toks.size() == 0){
			continue;
		}
		else if(toks[0] == "ln"){
			cout << "Partition name: '" << volume_label << "'.\n";
		}
		else if(toks[0] == "ls"){
			vector<string> returned = listdir(current_path);
			for(string filename : returned){
				cout << filename << " ";
			}
			cout << "\n";
		}
		else if(toks[0] == "cd"){
			expect(toks.size() > 1, "Directory must be specified.");
			string tmp001 = toks[1];
			string old_current_path = current_path;
			if(tmp001[0] == '/'){
				current_path = get_actual_path(tmp001);
			}
			else{
				if(current_path == "/"){
					current_path = get_actual_path(current_path + tmp001);
				}
				else{
					current_path = get_actual_path(current_path + "/" + tmp001);
				}
			}
			if(current_path == ""){
				current_path = old_current_path;
			}
		}
		else if(toks[0] == "cat"){
			expect(toks.size() > 1, "File must be specified.");
			string current_file = toks[1];
			if(current_path == "/"){
				current_file = current_path + current_file;
			}
			else{
				current_file = current_path + "/" + current_file;
			}
			uint64_t file_lba = get_path_lba(current_file);
			string temp = "";
			if(file_lba == 0){
				goto cat_end;
			}
			TUFS1FILEALLOC tmp;
			readsect(file_lba, &tmp);
			if(tmp.FSIBytes != 0){
				uint64_t fsize = tmp.FSIBytes;
				uint64_t excess = fsize % 503;
				uint64_t current_fsize = fsize - excess;
				TUFS1DATASECT datsec;
				readsect(tmp.FSSect, &datsec);
				if(current_fsize == 0){
					for(uint64_t i = 0; i < excess; i++){
						temp += (char)datsec.FRFData[i];
					}
				}
				else{
					for(uint64_t i = 0; i < 503; i++){
						temp += (char)datsec.FRFData[i];
					}
					current_fsize -= 503;
					while(1){
						readsect(datsec.NFRSect, &datsec);
						if(current_fsize == 0){
							for(uint64_t i = 0; i < excess; i++){
								temp += (char)datsec.FRFData[i];
							}
							break;
						}
						else{
							for(uint64_t i = 0; i < 503; i++){
								temp += (char)datsec.FRFData[i];
							}
							current_fsize -= 503;
						}
					}
				}
			}
			cout << temp << "\n";
			cat_end:
			;
		}
		else if(toks[0] == ""){
			continue;
		}
		else{
			cout << "ERROR: Unknown command " << toks[0] << ".\n";
		}
	}
	fclose(input_file);
	return 0;
}
