#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "bootsect.h"
#include "bpb.h"
#include "direntry.h"
#include "fat.h"
#include "dos.h"

// Laura Slade and Ashley Jones

/* We worked on just about everything together, Laura did most of array work to hold used cluster data. 

ERRORS THAT WE ARE GETTING!!! 

We are having severe issues with the orphans, we are able to detect them but when we try to create a directory entry for them (using create_dirent calling write_dirent) we encountered an issue : error is './scandisk' : free(): invalid next size (fast): followed by a string of hex numbers which were differnet each time. Using debugging statements we determined that this was happening at the line 'free (p2)' in write_dirent. We spent several (probably 7) hours trying to resolve this issue. We asked several classmates and Professor Stratton before finally posting to Piazza and recieving no response. No one could figure out what was happening. 

To avoid messing up the functionality of the other images we are commenting out the command to run the function that will create the directory entry.

We still did a fair amount of work for this question and hope you will take that into consideration since we have spent the better part of a day (and night) trying to resolve this issue. 

We also attempted to do functionality for 4 and 5 but due to orphan errors were unable to completely fix the disk images. 

We are going to leave in our debugging print statements so that you can potentially see were the issues are coming from. 

Have a great vacation!
*/


int delete_extra (uint16_t cluster, struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){
	//printf("in delete_extra\n");

	uint16_t cluster1 = cluster;
	if (!is_end_of_file(cluster1)){
		uint16_t fat_entry1 = get_fat_entry(cluster1, image_buf, bpb);
		while (!is_end_of_file(get_fat_entry(fat_entry1, image_buf, bpb))){
			cluster1 = fat_entry1;
			fat_entry1 = get_fat_entry(cluster1, image_buf, bpb); 
		}
		//free last cluster
		//printf ("fat_entry1 = %u\n", fat_entry1);
		set_fat_entry(fat_entry1, CLUST_FREE, image_buf, bpb);
		clust_used[fat_entry1] = 0;
		//printf("freed fat_entry1, it is now %u\n", fat_entry1);
		//set cluster1 to EOF
		//printf ("cluster1 = %u\n", cluster1);
		set_fat_entry (cluster1, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
		//printf("set cluster1 to EOF, it is now %u\n", cluster1);	
	}
	else {
		//free cluster1
		//set_fat_entry(cluster1, CLUST_FREE, image_buf, bpb);
		return	0  ;
		}
	//printf ("deleted some clusters\n");
	return 1 ;
}

int check_too_long (struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){
	uint16_t cluster = getushort(dirent->deStartCluster);
    uint32_t bytes_remaining = getulong(dirent->deFileSize);
    uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	int curr_size = cluster_size;	
	int is_deleted = 1;
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
		is_deleted = 0;
		while (!is_end_of_file(get_fat_entry(cluster, image_buf, bpb))){
			//printf("going to: delete_extra\n");
			//uint8_t clustval = malloc (sizeof (uint8_t));
			//clustval = cluster;
			//printf ("going to delet extras\n");
			is_deleted = delete_extra ( cluster, dirent, image_buf, bpb, clust_used);
			//set_fat_entry(cluster, get_fat_entry(clustval, image_buf, bpb), image_buf, bpb);
			
			
			}
		//set cluster = end of file.
		set_fat_entry (cluster, (FAT12_MASK & CLUST_EOFS), image_buf, bpb); 
		} 

	return is_deleted;
}

int truncate_file (struct direntry *dirent, int curr_size){
	printf ("trimming file size\n");
	putulong(dirent->deFileSize, curr_size);
	printf ("trimmed file size to %d\n", curr_size);
	return 1; 
	  

	}	

int check_length (struct direntry *dirent, uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used){

	uint16_t cluster = getushort(dirent->deStartCluster);
    uint32_t bytes_remaining = getulong(dirent->deFileSize);
    uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
	uint16_t next_cluster = get_fat_entry (cluster, image_buf, bpb);
	//int curr_size = cluster_size;
	int curr_size = 0;
	int changed = 0;
	int cut = 0;
	//breaking chains where 2 FAT entries point to same data. 
	if (clust_used[cluster] != 0){	
		set_fat_entry (cluster, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
		printf ("CUTTTTTT\n");
		changed =1;
		cut = 1;
		}
	clust_used[cluster] = 1;
	//if the head cluster is bad, free cluster. 
	if (cluster == (FAT12_MASK & CLUST_BAD)){
		set_fat_entry(cluster, CLUST_FREE, image_buf, bpb);
		printf ("bad cluster found, head \n");
		changed =1;
	}
	else {
		while (!is_end_of_file(cluster)){
			if (next_cluster == (FAT12_MASK & CLUST_BAD)){
				set_fat_entry(next_cluster, CLUST_FREE, image_buf, bpb);
				set_fat_entry (cluster, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
				printf ("bad cluster found, middle \n"); 
			}
			uint8_t prev_clust = cluster;
			cluster = get_fat_entry (cluster, image_buf, bpb);
			if (prev_clust == cluster) {
					printf("LOOOOOOOOOOOOPED\n");
					set_fat_entry(prev_clust, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
					changed = 1;
				}
			next_cluster = get_fat_entry (cluster, image_buf, bpb);
			clust_used[cluster] = 1;
			curr_size += cluster_size;
		}
	}
	if (cut ==1){
		if (curr_size > cluster_size){
			curr_size -= cluster_size;
		cut = 0;
		}
	}
	
	//printf("bytes_remaining is: %u\n",bytes_remaining);
	//printf("curr_size is: %d\n",curr_size);
	if ((curr_size - bytes_remaining)> cluster_size ) {  //<
		//printf ("chain too long!\n");
		changed = check_too_long (dirent, image_buf,  bpb, clust_used);
		}
	if (bytes_remaining > curr_size) {
		//printf ("file to big, going to truncate\n");
		changed = truncate_file (dirent, curr_size); //should always be 1 if it reaches this.
		}
	else {
		return changed;
		}
	return changed;	

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

	int value = check_length (dirent, image_buf, bpb, clust_used);
	//printf ("%d\n", value);
	if (value == 1){
		printf("%s.%s (%u bytes) (starting cluster %d) %c%c%c%c\n", 
	       name, extension, size, getushort(dirent->deStartCluster),
	       ro?'r':' ', 
               hidden?'h':' ', 
               sys?'s':' ', 
               arch?'a':' ');
		
	//printf("going to check_length\n");


		printf ("done checking! :)\n");
		}
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
	clust_used[cluster] = 3;//[get_fat_entry (cluster, image_buf, bpb)] = 3;
	printf ("cluster %d now is listed as %d\n", cluster, clust_used[cluster]);
	uint16_t next_cluster = get_fat_entry (cluster, image_buf, bpb);

	if (cluster == (FAT12_MASK & CLUST_BAD)){
		set_fat_entry(cluster, CLUST_FREE, image_buf, bpb);
		printf ("bad cluster found, head \n");
		}
	while (!is_end_of_file(cluster) ){
		if (next_cluster == (FAT12_MASK & CLUST_BAD)){
			set_fat_entry(next_cluster, CLUST_FREE, image_buf, bpb);
				set_fat_entry (cluster, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
				printf ("bad cluster found, middle \n"); 
			}
		cluster = get_fat_entry (cluster, image_buf, bpb);
		clust_used[cluster] = 2;//[get_fat_entry (cluster, image_buf, bpb)] = 2;
		next_cluster = get_fat_entry (cluster, image_buf, bpb);
		printf ("cluster %d now is listed as %d\n", cluster, clust_used[cluster]);
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
			if (clust_val != 0) {//CLUST_FREE){
				printf ("orphan found! going to find its tails.\n");
				orphan_tails (cluster, bpb, clust_used, dirent, image_buf);
				orphan_head_max ++;
				printf ("CLUSTER = %u\n", cluster);
				printf ("VAL AT INDEX = %d\n", clust_used[cluster]);
				
				
			}
		}
		cluster ++;
	}
	printf ("there are at most %d orphan chains \n", orphan_head_max);
	val = orphan_head_max;
	return val;
}

void build_head_list (uint8_t *clust_used, uint8_t *orphan_heads, uint8_t num_clust){
	uint8_t i = 2;
	int j = 0;
 	while (i < num_clust){	
		if (clust_used [i] == 3){
			orphan_heads [j] = i;
			j ++;
		}
		i ++;
	}
return;
}
	
/* write the values into a directory entry */
void write_dirent(struct direntry *dirent, char *filename, 
		  uint16_t start_cluster, uint32_t size)
{
    char *p, *p2;
    char *uppername;
    int len, i;

    /* clean out anything old that used to be here */
    memset(dirent, 0, sizeof(struct direntry));

    /* extract just the filename part */
    uppername = strdup(filename);
    p2 = uppername;
    for (i = 0; i < strlen(filename); i++) 
    {
	if (p2[i] == '/' || p2[i] == '\\') 
	{
	    uppername = p2+i+1;
	}
    }

    /* convert filename to upper case */
    for (i = 0; i < strlen(uppername); i++) 
    {
	uppername[i] = toupper(uppername[i]);
    }

    /* set the file name and extension */
    memset(dirent->deName, ' ', 8);
    p = strchr(uppername, '.');
    memcpy(dirent->deExtension, "___", 3);
    if (p == NULL) 
    {
	fprintf(stderr, "No filename extension given - defaulting to .___\n");
    }
    else 
    {
	*p = '\0';
	p++;
	len = strlen(p);
	if (len > 3) len = 3;
	memcpy(dirent->deExtension, p, len);
    }

    if (strlen(uppername)>8) 
    {
	uppername[8]='\0';
    }
    memcpy(dirent->deName, uppername, strlen(uppername));
    free(p2);

    /* set the attributes and file size */
    dirent->deAttributes = ATTR_NORMAL;
    putushort(dirent->deStartCluster, start_cluster);
    putulong(dirent->deFileSize, size);

    /* could also set time and date here if we really
       cared... */
}


/* create_dirent finds a free slot in the directory, and write the
   directory entry */

void create_dirent(struct direntry *dirent, char *filename, 
		   uint16_t start_cluster, uint32_t size,
		   uint8_t *image_buf, struct bpb33* bpb)
{
    while (1) 
    {
	if (dirent->deName[0] == SLOT_EMPTY) 
	{
	    /* we found an empty slot at the end of the directory */
	    write_dirent(dirent, filename, start_cluster, size);
	    dirent++;

	    /* make sure the next dirent is set to be empty, just in
	       case it wasn't before */
	    memset((uint8_t*)dirent, 0, sizeof(struct direntry));
	    dirent->deName[0] = SLOT_EMPTY;
	    return;
	}

	if (dirent->deName[0] == SLOT_DELETED) 
	{
	    /* we found a deleted entry - we can just overwrite it */
	    write_dirent(dirent, filename, start_cluster, size);
	    return;
	}
	dirent++;
    }
}

void build_orphanage (uint8_t *image_buf, struct bpb33* bpb, uint8_t *clust_used, int num_clust){	
	int i = 2;
	int n = 1;
	while (i < num_clust){
		if (clust_used[i] == 3) {
		uint16_t start_cluster =i;
		uint8_t cluster = i; 
		int clusters_needed = 1;
			while ((!is_end_of_file(cluster)) &&(is_valid_cluster (cluster, bpb))){
				printf("in while loop\n");
				clusters_needed +=1;
				printf("clusters_needed is: %d\n", clusters_needed);
				uint8_t prev_clust = cluster;
				cluster = get_fat_entry (cluster, image_buf, bpb);
				printf("next cluster is: %u\n", cluster);
				if (prev_clust == cluster) {
					set_fat_entry(prev_clust, (FAT12_MASK & CLUST_EOFS), image_buf, bpb);
				}
			}
		//if (clusters_needed >1){
			//clusters_needed -=1;
			//}
		printf ("we need to create a file with %d clusters \n", clusters_needed);
		char orphanage[32];

		//make the name of the file be "orphan#.dat"
		snprintf (orphanage, 32, "orphan%d.dat", n);
		
		uint16_t cluster_size = bpb->bpbBytesPerSec * bpb->bpbSecPerClust;
		uint32_t size = clusters_needed * cluster_size;

   	 struct direntry *dirent = (struct direntry*)cluster_to_addr( 0, image_buf, bpb);		
	
		//create orphanage file	
		printf("going to create %s\n", orphanage);
		printf("starting cluster is: %u\n", start_cluster);
		create_dirent(dirent, orphanage, start_cluster,size,image_buf, bpb);
		printf("created %s\n", orphanage);
		n++;
		}
	else {
		//printf ("false alarm!\n");
	}
	i++;
	}
	return;
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
	/*uint8_t *orphan_heads = malloc (sizeof (uint8_t) * val);
	for(int j = 0; j<val; j++) {
		orphan_heads[j] = 0;
	}

	int i = 0;
	build_head_list (clust_used, orphan_heads, num_clust);

	//for debugging purposes!
	for(int k = 0; k<val; k++) {
		printf ("orphan head is: %u\n", orphan_heads[k]);
	}
	 
	
	while (i<val){
		printf("Building orphanage for %d\n", i);
		printf("Building orphanage for cluster %u\n", orphan_heads[i]);
		build_orphanage (image_buf, bpb, orphan_heads, i);
		i++;
	}	
	*/
//	build_orphanage (image_buf, bpb, clust_used, num_clust);
	//free (clust_used); was causing an error when we tried to free, regardless of where we put the statement.
	
    unmmap_file(image_buf, &fd);
	
    return 0;
}
	
//for bad clusters: if (cluster == (FAT12_MASK & CLUST_BAD))

