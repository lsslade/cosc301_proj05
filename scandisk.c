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






void delete_extra (uint16_t cluster, struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){
	printf("in delete_extra\n");

	uint16_t cluster1 = cluster;
	if (!is_end_of_file(cluster1)){
		uint16_t fat_entry1 = get_fat_entry(cluster1, image_buf, bpb);
		while (!is_end_of_file(get_fat_entry(fat_entry1, image_buf, bpb))){
			cluster1 = fat_entry1;
			fat_entry1 = get_fat_entry(cluster1, image_buf, bpb); 
		}
		//free last cluster
		printf ("fat_entry1 = %u\n", fat_entry1);
		set_fat_entry(fat_entry1, CLUST_FREE, image_buf, bpb);
		clust_used[get_fat_entry (fat_entry1, image_buf, bpb)] = 0;
		printf("freed fat_entry1, it is now %u\n", fat_entry1);
		//set cluster1 to EOF
		printf ("cluster1 = %u\n", cluster1);
		set_fat_entry (cluster1, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
		printf("set cluster1 to EOF, it is now %u\n", cluster1);	
	}
	else {
		//free cluster1
		//set_fat_entry(cluster1, CLUST_FREE, image_buf, bpb);
		return ;
		}
	return ;
}

int check_too_long (struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){
	uint16_t cluster = getushort(dirent->deStartCluster);
    uint32_t bytes_remaining = getulong(dirent->deFileSize);
    uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	int curr_size = cluster_size;	
	//printf ("checking if too long\n");
	//while loop to check if a) cluster is valid
	while (curr_size<bytes_remaining){
		if (is_valid_cluster (cluster, bpb) || (!is_end_of_file(cluster))){
			//getting next fat entry.
			uint16_t fat_entry = get_fat_entry(cluster, image_buf, bpb);
			curr_size += cluster_size;
			cluster = fat_entry;
		}
		else {
			break;
			}
	}
	if (!is_end_of_file(cluster)){
		//printf ("chain is too long! it is %d, end cluster is %u\n", curr_size, get_fat_entry(cluster, image_buf, bpb));
	while (!is_end_of_file(get_fat_entry(cluster, image_buf, bpb))){
			//printf("going to: delete_extra\n");
			//uint8_t clustval = malloc (sizeof (uint8_t));
			//clustval = cluster;
			delete_extra ( cluster, dirent, image_buf, bpb, clust_used);
			//set_fat_entry(cluster, get_fat_entry(clustval, image_buf, bpb), image_buf, bpb);
			
			
			}
		//set cluster = end of file.
		set_fat_entry (cluster, (FAT12_MASK & CLUST_EOFS), image_buf, bpb); 
		} 

	return 0;
}

int truncate_file (struct direntry *dirent, int curr_size){
	printf ("trimming file size\n");
	putulong(dirent->deFileSize, curr_size);
	printf ("trimmed file size to %d\n", curr_size);
	return 0; 
	  

	}	

int check_length (struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){
	uint16_t cluster = getushort(dirent->deStartCluster);
    uint32_t bytes_remaining = getulong(dirent->deFileSize);
    uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	//int curr_size = cluster_size;
	int curr_size = 0;
	while (!is_end_of_file(cluster)){
		cluster = get_fat_entry (cluster, image_buf, bpb);
		curr_size += cluster_size;
		clust_used[get_fat_entry (cluster, image_buf, bpb)] = 1;
	}
	
	printf("bytes_remaining is: %u\n",bytes_remaining);
	printf("curr_size is: %d\n",curr_size);
	if ((curr_size - bytes_remaining)> cluster_size ) {  //<
		check_too_long (dirent, image_buf,  bpb, clust_used);
		}
	if (bytes_remaining > curr_size) {
		printf ("file to big, going to truncate\n");
		truncate_file (dirent, curr_size); 
		}
	else {
		return 1;
		}
	return 0;	

}
	
	
void print_indent(int indent)
{
    int i;
    for (i = 0; i < indent*4; i++)
	printf(" ");
}

//need to delete a bunch of stuff. and add checks
uint16_t print_dirent(struct direntry *dirent, int indent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used)
{
    uint16_t followclust = 0;

    int i;
    char name[9];
    char extension[4];
    uint32_t size;
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
	//printf("Volume: %s\n", name);
    } 
    else if ((dirent->deAttributes & ATTR_DIRECTORY) != 0) 
    {
        // don't deal with hidden directories; MacOS makes these
        // for trash directories and such; just ignore them.
	if ((dirent->deAttributes & ATTR_HIDDEN) != ATTR_HIDDEN)
        {
	   // print_indent(indent);
    	   // printf("%s/ (directory)\n", name);
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
	//print_indent(indent);
	printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", 
	       name, extension, size, getushort(dirent->deStartCluster),
	       ro?'r':' ', 
               hidden?'h':' ', 
               sys?'s':' ', 
               arch?'a':' ');
	//printf("going to check_length\n");
	int value = check_length (dirent, image_buf, bpb, clust_used);
	//printf ("going to : check_too_long\n");
	//check_too_long (dirent, image_buf, bpb);
	//printf("value is: %d\n",value);
	printf ("done checking! :)\n");
	//check_too_short;
    }

    return followclust;
}

//copy this --recursive!
void follow_dir(uint16_t cluster, int indent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used)
{
    while (is_valid_cluster(cluster, bpb))
    {
        struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

        int numDirEntries = (bpb->bpbBytesPerSec * bpb->bpbSecPerClust) / sizeof(struct direntry);
        int i = 0;
	for ( ; i < numDirEntries; i++)
	{
            
            uint16_t followclust = print_dirent(dirent, indent, image_buf, bpb, clust_used);
            if (followclust)
                follow_dir(followclust, indent+1, image_buf, bpb, clust_used);
            dirent++;
	}

	cluster = get_fat_entry(cluster, image_buf, bpb);
    }
}

//copy and paste this.
void traverse_root(uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used)
{
    uint16_t cluster = 0;

    struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);

    int i = 0;
    for ( ; i < bpb->bpbRootDirEnts; i++)
    {
        uint16_t followclust = print_dirent(dirent, 0, image_buf, bpb, clust_used);
        if (is_valid_cluster(followclust, bpb))
            follow_dir(followclust, 1, image_buf, bpb, clust_used);

        dirent++;
    }
}

void orphan_tails (uint16_t cluster, struct bpb33 *bpb, uint8_t *clust_used, struct direntry *dirent, uint8_t *image_buf){
	clust_used[get_fat_entry (cluster, image_buf, bpb)] = 3;
	while (!is_end_of_file(cluster)){
		cluster = get_fat_entry (cluster, image_buf, bpb);
		clust_used[get_fat_entry (cluster, image_buf, bpb)] = 2;
	}	
	return;

}

int find_orphans (uint8_t *clust_used, struct bpb33 *bpb, uint8_t num_clust, uint8_t *image_buf){
	printf ("in orphan_tails \n");
	uint16_t cluster = 2;
	//uint8_t *orphans = malloc(sizeof(uint8_t) * (num_clust));
	int val = 0;
    struct direntry *dirent = (struct direntry*)cluster_to_addr(cluster, image_buf, bpb);
	int orphan_head_max = 0;
	while (cluster < num_clust) { //traverse entrire FAT find orphans, val 0 = free, 1 = referenced by directory, 2 = referenced by other orphan, 3 = orphan head.
		uint16_t clust_val = get_fat_entry (cluster, image_buf, bpb);		
		if (clust_used[cluster] == 0){
			if (clust_val != CLUST_FREE){
				printf ("orphan found! going to find its tails.\n");
				orphan_tails (cluster, bpb, clust_used, dirent, image_buf);
				val = 1;
				orphan_head_max ++;
				printf ("cluster = %u\n", cluster);
				printf ("val at index = %d\n", clust_used[cluster]);
				
				
			}
		}
		cluster ++;
	}
	printf ("there are at most %d orphan chains \n", orphan_head_max);
	return val;
}
	
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
	//constants
	uint16_t num_sectors = 	bpb->bpbSectors;	/* total number of sectors */
	uint8_t sec_per_clust = bpb->bpbSecPerClust;	/* sectors per cluster */
	uint8_t num_clust = (num_sectors/sec_per_clust);
	uint8_t *clust_used = malloc(sizeof(uint8_t) * (num_clust));
	//images 1 and 2
	traverse_root(image_buf, bpb, clust_used);
	//image 3
	printf ("going to find orphans\n");
	int val = find_orphans (clust_used, bpb, num_clust, image_buf); 


    unmmap_file(image_buf, &fd);
    return 0;
}
	







