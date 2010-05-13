/**********************************************************************
* Author: Wei Wang (wwang@gwu.edu)
*
* Description: 
* Bound estimation for topk size 
*
* Last Modification Time: 05/12/2010
*
* Compile with:
* Type make.
*
* Usage:
* %s drill-down-times pathname theK
* 
**********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>     /* SIGINT */
#include <sys/stat.h>   /* stat() */
#include <unistd.h>
#include <time.h>    
#include <assert.h>     /* assert()*/

const long int MAX_DRILL_DOWN =  1000000000;
const long LIMIT_SET = 100000; /* Number of files sampled at most */
long int *file_size_array;
long int num_file_selected = 0;
int the_K = 0;

struct timeval start;
struct timeval end;
double est_total;
double est_num;
long int g_dir_visited = 0;
int g_reach_thresh = 0; /* flag whether reached the thresh */

/* New struct for saving history */
struct dir_node
{
	long int sub_file_num;
	long int sub_dir_num;

	double max_size;		/* size boundary */

	int bool_dir_covered;
	char *dir_name;
	char *dir_abs_path;	 /* absolute path, needed for BFS */
	struct dir_node *sdirStruct; /* child array dynamically allocated */
};
struct dir_node root;


void CleanExit(int sig);
void fast_subdirs(const char *path, struct dir_node *curPtr);
static char *dup_str(const char *s);
int begin_sample_from(const char *root, struct dir_node *);
int random_next(int random_bound);
int check_type(const struct dirent *entry);
long int get_file_size(const char *filename);
int compare (const void * a, const void * b);
int get_all_file(const struct dirent *entry);

int main(int argc, char **argv) 
{
	long int sample_times;
	size_t i;
	struct dir_node *curPtr;
	long int real_dir = 1;
	long int real_file = 1;
	char *root_abs_name = NULL;
	double the_k;   /* top-k in sample */
	
	signal(SIGKILL, CleanExit);
	signal(SIGTERM, CleanExit);
	signal(SIGINT, CleanExit);
	signal(SIGQUIT, CleanExit);
	signal(SIGHUP, CleanExit);

  
	/* start timer */
	gettimeofday(&start, NULL ); 

	if (argc < 4)
	{
		printf("Usage: %s drill-down-times pathname\n",argv[0]);
		printf("The-K [real_dir] [real_file] \n");
		return EXIT_FAILURE; 
	}
	if (argc >= 6)
	{
		real_dir = atol(argv[4]);
		real_file = atol(argv[5]);
	}
	if (chdir(argv[2]) != 0)
	{
		printf("Error when chdir to %s", argv[2]);
		return EXIT_FAILURE; 
	}

	the_K = atoi(argv[3]);

	printf("%s\t", get_current_dir_name());
	root_abs_name = dup_str(get_current_dir_name());

	sample_times = atoi(argv[1]);
	assert(sample_times <= MAX_DRILL_DOWN);
	printf("%ld\t", sample_times);

	/* initialize the value */
	{
        est_total = 0;
        est_num = 0;
		file_size_array = (long int *)
			malloc(sizeof(long int) * LIMIT_SET);
		if (file_size_array == NULL)
		{
			printf("file_size_array malloc error!\n");
			return EXIT_FAILURE;
		}
		memset(file_size_array, 0, LIMIT_SET);
	}
	
	/* Initialize the dir_node struct */
	curPtr = &root;
	curPtr->bool_dir_covered = 0;
	curPtr->sdirStruct = NULL;
	curPtr->dir_abs_path = dup_str(root_abs_name);
	
	srand((int)time(0));//different seed number for random function
	
	/* start sampling */
	for (i=0; i<sample_times && g_reach_thresh!=1; i++)
	{
		begin_sample_from(root_abs_name, curPtr);
	}
	
	/* so we drilled down i times */
	assert(i != 0);

	/* sort */
	/* now we have est_total and also the num_file_selected*/
	qsort(file_size_array, num_file_selected, 
			sizeof(long int), compare);



	printf("%.6f\t", abs (est_total/i - real_file) * 1.0 / real_file);
	printf("%.6f\t", g_dir_visited * 1.0 / real_dir);

	est_total = est_total / i;
	printf("est_tot:%lf\n", est_total);
	
	
    the_k = the_K * num_file_selected / est_total; 	
	printf("the_k:%f\n", the_k);
	if (the_k < 1)	the_k = 1.1;	
	printf("the_k adapted:%f\n", the_k);
	printf("the_k using real:%f\n", 
		1.0 * the_K * num_file_selected / real_file);
	/* display more than the_k */
	for (i=0; i<the_K*2-1; i++)
		printf("%d\t", file_size_array[i]);
	printf("\nNumber of files selected:%d\n", num_file_selected);
	CleanExit(2);
}

int compare (const void * a, const void * b)
{
  return -( *(long int*)a - *(long int*)b );
}


int random_next(int random_bound)
{
	assert(random_bound < RAND_MAX);
	return rand() % random_bound;	
}


int begin_sample_from(const char *root, struct dir_node *curPtr) 
{
	long int sub_dir_num = 0;
	long int sub_file_num = 0;
		
    //string curDict = dirstr;
	/* designate the current directory where sampling is to happen */
	char *cur_parent = dup_str(root);
	int bool_sdone = 0;
    double prob = 1;

		
    while (bool_sdone != 1)
    {
		sub_dir_num = 0;
		sub_file_num = 0;

		fast_subdirs(cur_parent, curPtr); 	
        
		sub_dir_num = curPtr->sub_dir_num;
		sub_file_num = curPtr->sub_file_num;
		
        est_total = est_total + (sub_file_num / prob);

		if (sub_dir_num > 0)
		{
			prob = prob / sub_dir_num;
			int temp = random_next(sub_dir_num);
			cur_parent = dup_str(curPtr->sdirStruct[temp].dir_name);
			curPtr = &curPtr ->sdirStruct[temp];		
		}
		
		/* leaf directory, end the drill down */
        else
        {
			est_num++;
			chdir(root);
			bool_sdone = 1;
        }
    } 
    return EXIT_SUCCESS;
}


static char *dup_str(const char *s) 
{
    assert(s != NULL);
    size_t n = strlen(s) + 1;
    char *t = malloc(n);
    if (t) 
	{
        memcpy(t, s, n);
    }
    return t;
}

void CleanExit(int sig)
{
    gettimeofday(&end, NULL ); 

    printf("%ld\n", 
	(end.tv_sec-start.tv_sec)*1+(end.tv_usec-start.tv_usec)/1000000);
    exit(0);
}


void fast_subdirs(   
    const char *path,               /* path name of the parent dir */
    struct dir_node *curPtr )	       
{
    struct dirent **namelist;
    struct dirent **file_namelist;

    size_t alloc;
    int total_num;
	int used = 0;
	long int sub_dir_num;		        /* number of sub dirs */
	long int sub_file_num;
	int temp = 0;
	int sdir_name_len;
	int dir_abs_len;

	/* already stored the subdirs struct before
	 * no need to scan the dir again */
	if (curPtr->bool_dir_covered == 1)
	{
		/* This change dir is really important */
		chdir(path);
		return;
	}
		
	/* so we have to scan */
	g_dir_visited++;	


	/* root is given like the absolute path regardless of the cur_dir */
    sub_dir_num = scandir(path, &namelist, check_type, 0);
	
	sub_file_num = scandir(path, &file_namelist, get_all_file, 0);
	chdir(path);
	
 	alloc = sub_dir_num - 2;
	used = 0;

 	assert(alloc >= 0);

    if (!(curPtr->sdirStruct
			= malloc(alloc * sizeof (struct dir_node )) )) 
	{
        //goto error_close;
		printf("malloc error!\n");
		exit(-1);
    }
     
    dir_abs_len = strlen((char*)get_current_dir_name());
	/* scan the namelist */
    for (temp = 0; temp < sub_dir_num; temp++)
    {
		if ((strcmp(namelist[temp]->d_name, ".") == 0) ||
                        (strcmp(namelist[temp]->d_name, "..") == 0))
               continue;
   		sdir_name_len = strlen(namelist[temp]->d_name);

        /* name only */
		curPtr->sdirStruct[used].dir_name = dup_str(namelist[temp]->d_name);

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
		used++;			//this is important to avoid segmentation fault !
		
	}

	sub_dir_num -= 2;


	for (temp = 0; temp < alloc; temp++)
	{
		curPtr->sdirStruct[temp].sub_dir_num = 0;
		curPtr->sdirStruct[temp].sub_file_num = 0;
		curPtr->sdirStruct[temp].bool_dir_covered = 0;
	}


	/* update info */
	curPtr->sub_file_num = sub_file_num;
	curPtr->sub_dir_num = sub_dir_num;
	curPtr->bool_dir_covered = 1;


	if (num_file_selected + sub_file_num > LIMIT_SET)
	{
		sub_file_num = LIMIT_SET - num_file_selected; 
		g_reach_thresh = 1;
		
	} 
	for (temp = 0; temp < sub_file_num; temp++)
	{
		assert(num_file_selected < LIMIT_SET);

		file_size_array[num_file_selected] = 
							get_file_size(file_namelist[temp]->d_name);
		num_file_selected++;	
		
	}


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

long int get_file_size(const char *filename)
{
	long size = 0;
	struct stat stat_buf;
	
	/* make sure to be in the correct directory */
	if (stat(filename, &stat_buf) != 0)
	{
		printf("stat error!\n");
		exit(-1);		
	}

	size = stat_buf.st_size;
	return size;	
}

