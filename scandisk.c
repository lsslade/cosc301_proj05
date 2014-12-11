#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"



void usage(char *progname) {
    fprintf(stderr, "usage: %s <imagename>\n", progname);
    exit(1);
}



int main(int argc, char** argv) {
    uint8_t *image_buf;
    int fd;
    struct bpb33* bpb;
    if (argc < 2) {
	usage(argv[0]);
    }

    image_buf = mmap_file(argv[1], &fd);
    bpb = check_bootsector(image_buf);


    // your code should start here...


    unmmap_file(image_buf, &fd);
    return 0;
}


void delete_extra (uint16_t cluster){
	uint16_t cluster1 = get_fat_entry(cluster, image_buf, bpb);
	if (!is_end_of_file(cluster1)){
		uint16_t fat_entry1 = get_fat_entry(cluster1, image_buf, bpb);
		while (!is_end_of_file(fat_entry1)){
			cluster1 = fat_entry1;
			fat_entry1 = get_fat_entry(cluster1, image_buf, bpb); 
		}
		//free fat_entry1
		//set cluster1 to EOF	
	}
	else {
		//free cluster1
		}
}

int check_too_long (struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb){
	uint16_t cluster = getushort(dirent->deStartCluster);
    uint32_t bytes_remaining = getulong(dirent->deFileSize);
    uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	int curr_size = cluster_size;	

	//while loop to check if a) cluster is valid
	while (curr_size<bytes_remaining){
		if (is_valid_cluster (cluster, bpb) || (!is_end_of_file(cluster) ){
			//getting next fat entry.
			uint16_t fat_entry = get_fat_entry(cluster, image_buf, bpb);
			curr_size += cluster_size;
			cluster = fat_entry;
		}
		else {
			break;
			{
	}
	if (!is_end_of_file(cluster){
		while (!is_end_of_file(get_fat_entry(cluster, image_buf, bpb)){
			delete_extra (cluster);
		}
	//set cluster = end of file. 
	}

	return 0;

}
	
	
void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent*4; i++)
	printf(" ");
}


uint16_t print_dirent(struct direntry *dirent, int indent,struct bpb33* bpb)
{
    uint16_t followclust = 0;

    int i;
    char name[9];
    char extension[4];
    uint32_t size;#include "dos.h"
    uint16_t file_cluster;
    name[8] = ' ';
    extension[3] = ' ';
    memcpy(name, &(dirent->deName[0]), 8);
    memcpy(extension, dirent->deExtension, 3);
    if (name[0] == SLOT_EMPTY)
    {
	return followclust;
    }

    /* skip over deleted entries */
    if (((uint8_t)name[0]) == SLOT_DELETED)
    {
	return followclust;
    }

    if (((uint8_t)name[0]) == 0x2E)
    {
	// dot entry ("." or "..")
	// skip it
        return followclust;
    }

    /* names are space padded - remove the spaces */
    for (i = 8; i > 0; i--) 
    {
	if (name[i] == ' ') 
	    name[i] = '\0';
	else 
	    break;
    }

    /* remove the spaces from extensions */
    for (i = 3; i > 0; i--) 
    {
	if (extension[i] == ' ') 
	    extension[i] = '\0';
	else 
	    break;
    }

    if ((dirent->deAttributes & ATTR_WIN95LFN) == ATTR_WIN95LFN)
    {
	// ignore any long file name extension entries
	//
	// printf("Win95 long-filename entry seq 0x%0x\n", dirent->deName[0]);
    }
    else if ((dirent->deAttributes & ATTR_VOLUME) != 0) 
    {
	printf("Volume: %s\n", name);
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
	if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
	    print_indent(indent);
    	    printf("%s/ (directory)\n", name);
            file_cluster = getushort(dirent->deStartCluster);
            followclust = file_cluster;
        }
    }
    else 
    {
        /*
         * a "regular" file entry
         * print attributes, size, starting cluster, etc.
         */
	int ro = (dirent->deAttributes & ATTR_READONLY) == ATTR_READONLY;
	int hidden = (dirent->deAttributes & ATTR_HIDDEN) == ATTR_HIDDEN;
	int sys = (dirent->deAttributes & ATTR_SYSTEM) == ATTR_SYSTEM;
	int arch = (dirent->deAttributes & ATTR_ARCHIVE) == ATTR_ARCHIVE;

	size = getulong(dirent->deFileSize);
	print_indent(indent);
	printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", 
	       name, extension, size, getushort(dirent->deStartCluster),
	       ro?'r':' ', 
               hidden?'h':' ', 
               sys?'s':' ', 
               arch?'a':' ');
    }

    return followclust;
}

//copy this --recursive!
void follow_dir(uint16_t cluster, int indent,
		uint8_t *image_buf, struct bpb33* bpb)
{
    while (is_valid_cluster(cluster, bpb))
    {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
	for ( ; i < numDirEntries; i++)
	{
            
            uint16_t followclust = print_dirent(dirent, indent);
            if (followclust)
                follow_dir(followclust, indent+1, image_buf, bpb);
            dirent++;
	}

	cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}

//copy and paste this.
void traverse_root(uint8_t *image_buf, struct bpb33* bpb)
{
    uint16_t cluster = 0;

    struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

    int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
        uint16_t followclust = print_dirent(dirent, 0, bpb);
        if (is_valid_cluster(followclust, bpb))
            follow_dir(followclust, 1, image_buf, bpb);

        dirent++;
    }
}

	
	
	//use get_entries to get list of FAT entries.
	/*int end = 0;
	while (end == 0){
		//check if data in slot, if so send to is_list_valid, handle accordingly
		//if no data in slot send to slot_empty.
			//if returns 0, set end to 1
			//else set pointer to next data spot to appropriate index given by is_slot_empty.
		//move pointer to next index in list. 
	}
*/






/*type(some kind of array)?*/ void get_entries (){//also not sure as to parameters
/*should be able to get the FAT entries and store them in list to be checked later. Will return pointer to list of entries. 
List entries will probs be of token variety. */
}

int is_slot_empty ()//not sure what to pass in as value will be &deName[0], not sure of type def.
/*helper funcion. Will check if you have reached end of directory,( i.e deName[0] contains SLOT_EMPTY)if so will return 0. if so, no more entries in directory. If deName[0] contains SLOT_DELETED then just keep moving through deName and return index number of next filled entry if exists, if next non deleted slot is the empty slot then will return 0.*/

int is_list_valid ()//agian, not sure as to type of params, will be int (size of file) and pointer to first block contianing data for that file.
/* will go through the list contained in the FAT for the different blocks containing file data. Will check if number of blocks used is what is needed to contain file. if yes, return 1, else, truncate list or file size appropriately and return 0 so we know change was made.
will truncate file if too few blocks found and will truncate list if too many blocks available. 
Will need ot mark remaining blocks on the list as deleted to avoid confusion later. 

*/






