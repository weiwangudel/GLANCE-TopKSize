/* Programmed by Wei Wang
 * Directed by Professor Howie Huang
 *
 * March 21, 2010
 *
 * Find TOP-K Objects in a filesystem
 ***********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>     /* SIGINT */
#include <sys/stat.h>   /* stat() */
#include <unistd.h>
#include <sys/time.h>   /* gettimeofday() */
#include <time.h>		/* time() */
#include <assert.h>     /* assert()*/
#include "level_queue.h"


#define MAX_PERMU 1000000
#define MAX_DRILL_DOWN 1000000
#define MAX_INT 100000000

struct dir_node
{
	long int sub_file_num;
	long int sub_dir_num;

	double min_size;		
	double max_size;		/* size boundary */

	int bool_dir_explored;
	char *dir_name;
	char *dir_abs_path;	 /* absolute path, needed for BFS */
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};

/* for time statistics */
struct timeval start;
struct timeval end;

/* command options */
int g_realsize = 0;		/* --realsize changes it to 1 */ 
double g_estratio = 10; /* how to extend the size of the encoutered dir */

double est_total;
long int sample_times;
long int qcost = 0;
long int individual_max_qcost = 0; /* records the maximum
									* number of directory traversed
									* by one individual range check */
long int individual_qcost = 0;
long int already_covered = 0;
long int newly_covered = 0;

/* MAC(modify, access, change) related topk */
int g_k_elem;
time_t g_prog_start_time;


int root_flag = 0; /* only set root factor to 1 in fast_subdir */

int ar[MAX_PERMU];		/* used for permutation */

struct queueLK level_q;
struct queueLK tempvec;

/********************************************************/
/******* Drill down depth suppose within 200 (level < 200)   *****/
int g_level = 0;  
int g_level_thresh = 8;
struct dir_node *g_depth_stack[200];
int g_stack_top = 0;
double saved_min_size;
double saved_max_size;
double saved_low_size;  /* for `binary search` */
double saved_high_size; /* for `binary search` */
double topk_min_size;
double topk_max_size;
unsigned int    topk_dir_num = 1;
unsigned int    the_K;
unsigned int    cur_k;
int has_boundary_flag = 0;
/*****************************************************************/



/***********************functions ********************************/
void CleanExit(int sig);
static char *dup_str(const char *s);
int n_begin_estimate_from(struct dir_node *rootPtr);
int check_type(const struct dirent *entry);
void n_fast_subdirs(struct dir_node *curDirPtr);
long int new_count_for_topk(int argc, char* argv[]);

/* why do I have to redefine to avoid the warning of get_current_dir_name? */
char *get_current_dir_name(void);
double floor(double);
int eligible_subdirs(struct dir_node sub_dir_ptr);
int record_dir_output_file(struct dir_node *curPtr);
void get_subdirs(const char *path, struct dir_node *curPtr);	
int o_begin_sample_from(const char *sample_root, struct dir_node *curPtr);
void set_range(int top);
int get_eligible_file(const struct dirent *entry);
void collect_topk(struct dir_node *rootPtr);
int old_count_for_topk(int argc, char **argv);
int get_all_file(const struct dirent *entry);

int min(int a, int b);
int max(int a, int b);
int Random(int left, int right);

void swap(int *a, int *b);
void permutation(int size);

int main(int argc, char* argv[]) 
{
	
	time(&g_prog_start_time);	
	
	old_count_for_topk(argc, argv);
	
	return EXIT_SUCCESS;
}


int old_count_for_topk(int argc, char **argv) 
{
	unsigned int sample_times;
	size_t i;
	struct dir_node *curPtr;
	struct dir_node root;
	
	
	printf("hi!");

	char *root_abs_name = NULL;
	
	signal(SIGKILL, CleanExit);
	signal(SIGTERM, CleanExit);
	signal(SIGINT, CleanExit);
	signal(SIGQUIT, CleanExit);
	signal(SIGHUP, CleanExit);
  
	/* start timer */
	gettimeofday(&start, NULL ); 

	if (argc < 6)
	{
		printf("Usage: %s\n pilot-drill-down-times pathname ",argv[0]);
		printf("topk_min_size topk_max_size dir_number [value of K]\n");
		printf("[--realsize (must follow value K)]\n");
		return EXIT_FAILURE; 
	}
	if (chdir(argv[2]) != 0)
	{
		printf("Error when chdir to %s", argv[2]);
		return EXIT_FAILURE; 
	}
	
	/* support relative path for command options */
	if (root_abs_name != NULL)
	{
		//free(root_abs_name);
	}
	
	root_abs_name = dup_str(get_current_dir_name());

	sample_times = atoi(argv[1]);
	assert(sample_times <= MAX_INT);

	assert(argv[3] != NULL);
	assert(argv[4] != NULL);
		
	topk_min_size = atof(argv[3]);
	topk_max_size = atof(argv[4]); 

	topk_dir_num = atoi(argv[5]);
	if (argc == 7)
		the_K = atoi(argv[6]);	
	if (argc == 8)
	{
		if (strcmp(argv[7], "--realsize") == 0)
		{
			g_realsize = 1;
		}
			
	}
	
	/* Initialize the dir_node struct */
	curPtr = &root;
	curPtr->bool_dir_explored = 0;
	curPtr->sdirStruct = NULL;
	curPtr->dir_abs_path = dup_str(root_abs_name);
	
	srand((int)time(0));//different seed number for random function

	
	printf("hi!");
	/* start sampling to get size range information */
	for (i=0; i < sample_times; i++)
	{
		o_begin_sample_from(root_abs_name, curPtr);
		curPtr = &root;
	}

	printf("begin sample from passed!\n");

	cur_k = 0;
	saved_low_size = topk_min_size;
	saved_high_size = topk_max_size;
	has_boundary_flag = 0;

	/* if the return of a certain range equals to the_k,
	   or the return of the certain range doesn't equal to
       (in other words, cannot equal to the_k) but the 
	   top_min_size has got equal to top_max_size) */
    while (cur_k != the_K && 
		abs(saved_low_size - saved_high_size) > 0.1  ) 
	{	
		printf("cur_k:%d, the_K:%d, low_size:%.2f, high_size:%.2f,\
topk_max:%.2f\n", cur_k, the_K, saved_low_size,
			 saved_high_size, topk_max_size);
	    cur_k = 0;
	
		/* get whatever the result of given range is */
		collect_topk(&root);

		/* update query range */
		if (cur_k > the_K)
		{
			has_boundary_flag = 1;
			/* if becomes cur_k < the_K, facilitate restoration */
			saved_high_size = topk_max_size;
			topk_max_size = (saved_low_size + topk_max_size) / 2;
		}
		else
		{	
			saved_low_size = topk_max_size;

			/* there are two cases when cur_k < the_K:
			 * 1) have encounter cur_k > the_K. there exists clear boundary
			 * 2) have not. No clear boundary exist, double to detect boundary
			 */
			
			/* exactly equal */
			if (has_boundary_flag  == 0 )
			{
				topk_max_size *= 2;
				saved_high_size = topk_max_size;
			}
			
			/* saved_high_size is the clear boundary, and is larger than 
			 * topk_max_size  
			 */
			else
				topk_max_size = (saved_high_size + topk_max_size) / 2;
			
		}
		/* deal with coverage */
		if (individual_qcost > individual_max_qcost)
		{
			individual_max_qcost = individual_qcost;
		}
		individual_qcost = 0;
	    	
	}
	
	printf("cur_k:%d, the_K:%d, low_size:%.2f, high_size:%.2f, topk_max:%.2f\n",
			cur_k, the_K, saved_low_size, saved_high_size, topk_max_size);
    
	/* Exit and Display Statistic */
	CleanExit (2);
	return EXIT_SUCCESS;
}

int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}

int o_begin_sample_from(
		const char *sample_root, 
		struct dir_node *curPtr) 
{
	int sub_dir_num;
		
	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(sample_root);
	int bool_sdone = 0;
		
    while (bool_sdone != 1)
    {
		/* stack in */
		g_depth_stack[g_stack_top] = curPtr;
		g_stack_top++;
		
		get_subdirs(cur_parent, curPtr);
		
		sub_dir_num = curPtr->sub_dir_num;
		
		/* drill down according to reported sub_dir_num */
		if (sub_dir_num > 0)
		{
			/*......deleted divide and conquer.......*/

			/* how to do random rejection to boost randomness 
			 * to avoid choosing leaf again?  */
			int temp = random_next(sub_dir_num);				
			if (cur_parent != NULL)
			{
				//free(cur_parent);
			}
			cur_parent = dup_str(curPtr->sdirStruct[temp].dir_abs_path);
			curPtr = &curPtr ->sdirStruct[temp];
			
		}
		
		/* leaf directory, end the drill down */
        else
        {
			assert(g_stack_top - 1 >= 0);
			saved_min_size = g_depth_stack[g_stack_top - 1]->min_size;
			saved_max_size = g_depth_stack[g_stack_top - 1]->max_size;

			//printf("saved_min_size:%f, saved_max_size:%f\n", 
			  //      saved_min_size, saved_max_size);
			/* backtracking */
			do 
			{
				set_range(g_stack_top - 1);
				g_stack_top--;			/* stack out */
				          
			} while (g_stack_top > 0);
			
			/* finishing this drill down, set the direcotry back */
			if (chdir(sample_root) != 0)
			{
				printf("chdir to sample_root failed\n");
				exit(-1);
			}
			bool_sdone = 1;
        }
    } 
    return EXIT_SUCCESS;
}


void get_subdirs(   
    const char *path,               /* path name of the parent dir */
    struct dir_node *curPtr)
{
    struct dirent **namelist;
	struct dirent **file_namelist;    
    size_t alloc;
	int used = 0;
	struct stat stat_buf;
	int sub_dir_num;		        /* number of sub dirs */
	int sub_file_num;
	int dir_abs_len;	/* length of the path */
	int sdir_name_len;

	/* already stored the subdirs struct before
	 * no need to scan the dir again */
	if (curPtr->bool_dir_explored == 1)
	{
		/* This change dir is really important */
		already_covered++;			
		return;
	}

	/* chdir for getting sub_dir's and files */
	if (chdir(path) != 0)
	{
		printf("chdir failed\n");
		exit(-1);
	}
		
	/* so we have to scan */
	newly_covered++;
	
	/* root is given like the absolute path regardless of the cur_dir */
    sub_dir_num = scandir(path, &namelist, check_type, 0);
	
	
 	alloc = sub_dir_num - 2;
	used = 0;

    if (!(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node)) )) 
	{
		printf("malloc error!\n");
		exit(-1);
    }
    
    int temp = 0;

	for (temp = 0; temp < alloc; temp++)
	{
		curPtr->sdirStruct[temp].sub_dir_num = 0;
		curPtr->sdirStruct[temp].sub_file_num = 0;
		curPtr->sdirStruct[temp].bool_dir_explored = 0;
		curPtr->sdirStruct[temp].min_size = 0;
		curPtr->sdirStruct[temp].max_size = 0;
	}	
	
	dir_abs_len = strlen(path);
	
	/* scan the namelist */
    for (temp = 0; temp < sub_dir_num; temp++)
    {
		if ((strcmp(namelist[temp]->d_name, ".") == 0) ||
                        (strcmp(namelist[temp]->d_name, "..") == 0))
               continue;
		
		sdir_name_len = strlen(namelist[temp]->d_name);
		
		curPtr->sdirStruct[used].dir_abs_path =
			(char*) malloc((dir_abs_len + 1   /* for "/" */
					+ sdir_name_len +1) * sizeof(char));
		if ( NULL == curPtr->sdirStruct[used].dir_abs_path)
		{
			printf("malloc error!\n");
			exit(-1);
		}
		strcpy(curPtr->sdirStruct[used].dir_abs_path, 
				curPtr->dir_abs_path);
		strcat(curPtr->sdirStruct[used].dir_abs_path, "/");
		strcat(curPtr->sdirStruct[used].dir_abs_path, 
				namelist[temp]->d_name); 
		
	}
	sub_dir_num -= 2;
	curPtr->sub_dir_num = sub_dir_num;
	
	/* update bool_dir_explored info */
	curPtr->bool_dir_explored = 1;

	

	/* encounter a new folder, update the [min, max] range 
	 * corresponding to matlab preprocessing dtarr(i+1) stage 
 	 * in supertime.m matlab code (should be supersize.m actually */
	
	/* get the name list */
	sub_file_num = scandir(path, 
	                      &file_namelist, get_all_file, 0);
	
	/* use real size, should use stat */
	if (g_realsize == 1)  
	{	
		int temp_i = 0;
		for (temp_i=0; temp_i < sub_file_num; temp_i++)
		{
			if (stat(file_namelist[temp_i]->d_name, &stat_buf) != 0)
			{
				printf("stat error!\n");
				exit(-1);		
			}
	
			if (stat_buf.st_size > curPtr->max_size)
			{
				curPtr->max_size = stat_buf.st_size;
			}
		} //end for 
	}
	else   /* read from file */
	{		
		FILE *fgetsize;
		char size_buf[30];
		double st_size;
		int temp_i = 0;

		for (temp_i=0; temp_i < sub_file_num; temp_i++)
		{
			if ((fgetsize = fopen(file_namelist[temp_i]->d_name, "r")) == 0)
			{
				printf("fopen error!\n");
				exit(-1);		
			}
			if ( NULL == fgets(size_buf, 30, fgetsize)) 
			{
				printf("error read size info from file\n");
			}	
			st_size = atof(size_buf);
			
			if (st_size > curPtr->max_size)
			{
				curPtr->max_size = st_size;
			}
		} //end for 
	}

}

void set_range(int top)
{
	if (g_depth_stack[top]->min_size > saved_min_size)
		g_depth_stack[top]->min_size = saved_min_size;

	if (g_depth_stack[top]->max_size < saved_max_size)
		g_depth_stack[top]->max_size = saved_max_size;

}

/* traverse to collect topk */
void collect_topk(struct dir_node *rootPtr)
{
	struct dir_node *cur_dir = NULL;
	            
    /* root_dir goes to queue */
    enQueue(&level_q, rootPtr);
    

    /* if queue is not empty */
    while (emptyQueue(&level_q) != 1)    
    {
        g_level++;
        initQueue(&tempvec);
		
        /* for all dirs currently in the queue 
         * these dirs should be in the same level 
         */
        for (; emptyQueue(&level_q) != 1; )
        {
            cur_dir = outQueue(&level_q);
			
			/* find the eligible dirs and files for record (into queue)
			 * and output (to display */
			int early_quit;
			early_quit = record_dir_output_file(cur_dir);
			qcost++;			/* number of directories covered increment */
		    individual_qcost++;
	
			/* cleanup queue and be ready to start all over again 
			 * but not reset qcost as binary search will incur more cost
             * and need to be counted in 
			 */
			if (early_quit == 1)  /* meaning the results are more than 2K */
			{
				clearQueue(&tempvec);
				clearQueue(&level_q);
				return;				
			}
				
        }
		
		/* with initQueue(tempvec) later, 		
		 * two queues are switched, initQueue won't free 
		 */
		level_q.front = tempvec.front;
		level_q.rear = tempvec.rear;
    }
}


int record_dir_output_file(struct dir_node *curPtr)
{
	int i;
    struct dirent **dir_namelist;
	struct dirent **file_namelist;
	long int sub_dir_num;
	long int sub_file_num;
	long int used;
	long int alloc;
	int dir_abs_len = 0;
	int sdir_name_len = 0;
	
	/* if the directory has been explored, meaning the subdir struct are
	 * there for use */	
	if (curPtr->bool_dir_explored == 1)
	{
		if (g_level > g_level_thresh || 
			 curPtr->max_size * 10 > 10000000)
		{
			/* add all the sub dirs to the current queue */
			for (i = 0; i < curPtr->sub_dir_num; i++)
			{
				enQueue(&tempvec, &curPtr->sdirStruct[i]);
			}		
		}
	}

	/* this directory has never been drilled down, we need to explore 
	 * its sub_dirs (malloc for future traverse) and put it to the queue  
	 */
	else
	{
		/* for absolute name */
		if (chdir(curPtr->dir_abs_path) != 0)
		{
			printf("chdir failed\n");
		}
		
		sub_dir_num = scandir(curPtr->dir_abs_path, 
		                      &dir_namelist, check_type, 0);
		
 		alloc = sub_dir_num - 2;
		used = 0;

  		if (alloc > 0 && !(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node)) )) 
		{
		    //goto error_close;
			printf("malloc error!\n");
			exit(-1);
		}
		
		int temp = 0;

		for (temp = 0; temp < alloc; temp++)
		{
			curPtr->sdirStruct[temp].sub_dir_num = 0;
			curPtr->sdirStruct[temp].sub_file_num = 0;
			curPtr->sdirStruct[temp].bool_dir_explored = 0;
		}
		
		dir_abs_len = strlen(curPtr->dir_abs_path);
		/* scan the namelist */
		for (temp = 0; temp < sub_dir_num; temp++)
		{
			if ((strcmp(dir_namelist[temp]->d_name, ".") == 0) ||
		                    (strcmp(dir_namelist[temp]->d_name, "..") == 0))
		           continue;
	   		if (!(curPtr->sdirStruct[used].dir_name 
				   = dup_str(dir_namelist[temp]->d_name))) 
			{
				printf("get name error!!!!\n");
			}	
			
			sdir_name_len = strlen(dir_namelist[temp]->d_name);
			
			curPtr->sdirStruct[used].dir_abs_path =
				(char*) malloc((dir_abs_len + 1   /* for "/" */
							+ sdir_name_len +1) * sizeof(char));
			strcpy(curPtr->sdirStruct[used].dir_abs_path, 
					curPtr->dir_abs_path);
			strcat(curPtr->sdirStruct[used].dir_abs_path, "/");
			strcat(curPtr->sdirStruct[used].dir_abs_path, 
					dir_namelist[temp]->d_name); 
			

			/* also queue in */
			enQueue(&tempvec, &curPtr->sdirStruct[used]);
			used++;
		}

		sub_dir_num -= 2;
		

		curPtr->sub_dir_num = sub_dir_num;
	
		/* update bool_dir_explored info */
		curPtr->bool_dir_explored = 1;		
		
	}
	
	/* check all the file's size info under the curPtr->dirname
	 * and output those are eligible (within the topk range
	 */
	if (chdir(curPtr->dir_abs_path) != 0)
	{
		printf("chdir failed\n");
		exit(-1);
	}
	sub_file_num = scandir(curPtr->dir_abs_path, &file_namelist,
	                       get_eligible_file, 0);
	//printf("eligible file numbers: %ld\n", sub_file_num);
	for (i = 0; i < sub_file_num; i++)
	{
		cur_k++;
		printf("%s/%s\n", curPtr->dir_abs_path, file_namelist[i]->d_name);	
		if (cur_k > 1.2 * the_K)
		{
			return 1;
		}				    	
	}
	return 0;
	
}

int eligible_subdirs(struct dir_node sub_dir)
{
	if (topk_max_size < sub_dir.max_size)
	{
		return 0;		
	}
	
	return 1;
}

static char *dup_str(const char *s) 
{
    size_t n = strlen(s) + 1;
    char *t = malloc(n);
    if (t) 
	{
        memcpy(t, s, n);
    }
    return t;
}

int check_type(const struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
        return 1;
    else
        return 0;
}
int get_all_file(const struct dirent *entry)
{
    if (entry->d_type == DT_DIR)
        return 0;
    else
        return 1;
}

/* Get files that are within the query */
int get_eligible_file(const struct dirent *entry)
{
	struct stat stat_buf;

	/* only consider the files, because it is size topk */
	if (entry->d_type == DT_DIR)
		return 0;
	
	if (g_realsize == 1)  
	{	
		/* make sure to be in the correct directory */
		if (stat(entry->d_name, &stat_buf) != 0)
		{
			printf("stat error!\n");
			exit(-1);		
		}

		if (stat_buf.st_size > 10000000)
		{
			return 1;
		}
		else
			return 0;
	}
	else   /* read from file */
	{		
		FILE *fgetsize;
		char size_buf[30];
		double st_size;

		if ((fgetsize = fopen(entry->d_name, "r")) == 0)
		{
			printf("fopen error!\n");
			exit(-1);		
		}
		fgets(size_buf, 30, fgetsize);	
		st_size = atof(size_buf);
		if (st_size > 10000000)
		{
			return 1;
		}
		else 
			return 0;
	}

}


/************************SIMPLE MATH WORK**********************************/
int min(int a, int b)
{
    if (a < b)
        return a;
    return b;
}

int max(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

void permutation(int size)
{
  assert(size < MAX_PERMU);
  int i;
  for (i=0; i < size; i++)
    ar[i] = i;
  for (i=0; i < size-1; i++)
    swap(&ar[i], &ar[Random(i,size)]);
}

void swap(int *a, int *b)
{
    int t;
    t=*a;
    *a=*b;
    *b=t;
}


int Random(int left, int right)
{
	return left + rand() % (right-left);
}


/******************EXIT HANDLING FUNCTION*****************************/
void CleanExit(int sig)
{
    /* make sure everything that needs to go to the screen gets there */
    fflush(stdout);
    gettimeofday(&end, NULL ); 
    

    if (sig != SIGHUP)
        printf("\nExiting...\n");
    else
        printf("\nRestarting...\n");


    puts("\n\n=============================================================");
	printf("\n%ld dirs traversed, coverage: %f\n", qcost,
	       			qcost*1.0/topk_dir_num);
	printf("%ld dirs traversed in one find topK, coverage: %f\n",
		individual_max_qcost, individual_max_qcost*1.0/topk_dir_num);
    puts("=============================================================");
    printf("Total Time:%ld milliseconds\n", 
	(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000);
    printf("Total Time:%ld seconds\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);

	
	printf("%ld\n", 
    (end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);


	exit(0);
}

