/*
 * Bit2Bin - strip .bit header and align binary to 16K.
 *
 * Copyright (C) 2019-2023 Antonio Villena
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM "Bit2Bin_zx3"
#define DESCRIPTION "strip .bit header and split binary to 1152Kb files"
#define VERSION "0.10 (2023-10-05)"
#define COPYRIGHT "Copyright (C) 2019-2023 Antonio Villena"
#define LICENSE \
"This program is free software: you can redistribute it and/or modify\n" \
"it under the terms of the GNU General Public License as published by\n" \
"the Free Software Foundation, version 3."
#define HOMEPAGE "https://github.com/zxdos/zxuno/"


#define SPLIT_SIZE 0x120000 // 1179648 bytes (1152 Kb)

FILE *fi, *fo;
int i, length;
unsigned char mem[0x4000*4]; // 64Kb por si hay chunks > 16K
unsigned short j;


void show_help() {
  printf(
    PROGRAM " version " VERSION " - " DESCRIPTION "\n"
    COPYRIGHT "\n"
    LICENSE "\n"
    "Home page: " HOMEPAGE "\n"
    "\n"
    "Usage:\n"
    "  " PROGRAM "        <input_file> [<output_file>]\n"
    "\n"
    "  <input_file>   Input BIT file\n"
    "  <output_file>  Output BIN file\n"
    "\n"
    /*"All parameters are mandatory.\n"*/
  );
}


const char *get_filename_from_path(const char *path)
{
	int i = strlen(path);
	while (i-- > 0)
	{
		if (path[i] == '\\' || path[i] == '/' || path[i] == ':')
			return path+i+1;
	}
	return path;
}


const char *get_extens_from_path(const char *path)
{
	const char *name = get_filename_from_path(path);
	const char *ext = strrchr(name,'.');
	return ext ? ext : name+strlen(name);
}


int fread_length(FILE *f)
{
	unsigned char lo,hi;
	unsigned short len;
	fread(&hi, 1, 1, f);
	fread(&lo, 1, 1, f);
	len = (lo|(hi<<8));
	return len;
}


int fread_chunk(FILE *f, unsigned char *p, int extra)
{
	int len = fread_length(f);
	fread(p,1,len+extra,f);
	return len+extra+2;
}


int write_split(FILE *f, char *filename, int splitsize)
{
	FILE *fout = fopen(filename, "wb");
	if (!fout)
	{
		printf("Cannot create output file: %s\n", filename);
		exit(-1);
	}

	for (int i=0; i<splitsize; i++)
	{
		unsigned char d = 0;
		if (!feof(f) && i+1024<splitsize)
			i+=fwrite(mem,1,fread(mem,1,1024,f),fout);
		if (!feof(f))
			fread(&d,1,1,f);
		fwrite(&d,1,1,fout);
	}

	fclose(fout);
	return 0;
}


int main(int argc, char *argv[]) {
  if( argc==1 )
    show_help(),
    exit(0);
  if( argc<2 )
    printf("Invalid number of parameters\n"),
    exit(-1);
  fi= fopen(argv[1], "rb");
  if( !fi )
    printf("Input file not found: %s\n", argv[1]),
    exit(-1);
  fseek(fi, 0, SEEK_END);
  i= ftell(fi);
  fseek(fi, 0, SEEK_SET);

  printf(PROGRAM " version " VERSION "\n");

  printf("input: %s (%d bytes)\n", argv[1], i);

  int len = fread_length(fi);
  fseek(fi, 0, SEEK_SET);

  if (len == 0xffff)
  {
    printf("input is not a .bit file, will be processed as .bin\n");
    length = i;
  }
  else
  {
    i -= fread_chunk(fi, mem, 0);
    i -= fread_chunk(fi, mem, 0);
    i -= fread_chunk(fi, mem, 1);
    printf(" info: %s\n",mem);
    i -= fread_chunk(fi, mem, 1);
    printf(" id: %s\n",mem);
    i -= fread_chunk(fi, mem, 1);
    i -= fread_chunk(fi, mem, 1);
    i -= fread(mem, 1, 4, fi);
    length = mem[3]|mem[2]<<8|mem[1]<<16|mem[0]<<24;
  }

  if( i!=length )
    printf("Invalid file length\n"),
    exit(-1);

  printf("input binary has: %d blocks of 16Kb, and %d bytes\n", length>>14, length&0x3fff);

  const char *path = argc>2 ? argv[2] : argv[1];
  const char *name = get_filename_from_path(path);
  const char *extens = get_extens_from_path(path);
  const char *newextens = argc>2 && *extens ? extens : ".zx3";
  char outname[260];

  if(1) // write full bin aligned to 16Kb
  {
    int ipos = ftell(fi);
    sprintf(outname, "%.*s%.*s_[full]%s", name-path, path, extens-name, name, newextens);
    printf("writing full: %s\n", outname);
    write_split(fi, outname, (length+0x3fff)&~0x3fff);
    fseek(fi, ipos, SEEK_SET);
    if (ipos != ftell(fi))
        printf("fseek error\n"),
        exit(-1);
  }


  int number=0;
  while(!feof(fi)) // write splits aligned to 1152Kb
  {
    sprintf(outname, "%.*s%.*s_[split_%02d]%s", name-path, path, extens-name, name, 1+number++, newextens);
    printf("writing split: %s\n", outname);
    write_split(fi, outname, SPLIT_SIZE);
  }

  //printf("%d file(s) successfully created\n", number);
  fclose(fi);

#if 0
  fo= fopen(argv[2], "wb+");
  if( !fo )
    printf("Cannot create output file: %s\n", argv[2]),
    exit(-1);
  j= i>>14;
  if( j )
    for ( i= 0; i<j; i++ )
      fread(mem, 1, 0x4000, fi),
      fwrite(mem, 1, 0x4000, fo);
  memset(mem, 0, 0x4000);
  fread(mem, 1, length&0x3fff, fi),
  fwrite(mem, 1, 0x4000, fo);
  if( j<71 ){
    memset(mem, 0, 0x4000);
    if( j>48 )  // ZX3
      for ( i= 0; i<71-j; i++ )
        fwrite(mem, 1, 0x4000, fo);
    else if( j>28 )  // ZXD
      for ( i= 0; i<48-j; i++ )
        fwrite(mem, 1, 0x4000, fo);
    else if( j>20 )  // ZX2
      for ( i= 0; i<28-j; i++ )
        fwrite(mem, 1, 0x4000, fo);
    else  // ZX1
      for ( i= 0; i<20-j; i++ )
        fwrite(mem, 1, 0x4000, fo);
  }
  printf("File `%s' successfully created\n", argv[2]);
#endif
}
