/*
Author: Zach Grimes
This program simulates a multithreaded scheduler. 

Compile instructions:
enter into terminal: g++ -std=c++11 -pthread ./zach_grimes_assignment3.cpp -o makename; ./makename < yourtestcases.txt
*/
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <iostream>
#include <sstream>
#include <iterator>
#include <string>
#include <vector>
#include <queue>
#include <fstream>
using namespace std;

struct user_info {
	int usr_num,
	 	usr_grp,
		res_req,
		init_t,
		req_t;
};

static pthread_mutex_t gsem; // mutex for group lock
static pthread_mutex_t rsem; // mutex for resource lock
static vector<int> positions={0,0,0,0,0,0,0,0,0,0}; //keeps track of the resource(s) specifically in use.
static vector<pthread_cond_t> res_conds={PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER,PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};
/* note that this previous line seems uneccessarily large, I attempted to push the 
   (pthread_cond_t=PTHREAD_COND_INITIALIZER) via looping through each ten
   spots in the condition vector, but it does not initialize each spot to the initializer 
   like it does on the previous line. 
*/
vector<user_info> user_reqs; // a list containing each request and thread data - in order specified by text file
static vector<queue<int> > grp_stager;
static int cur_usr_grp;
static int num_grp_cyles=1;
static pthread_cond_t usr_grp_inc = PTHREAD_COND_INITIALIZER;

static vector<int> req_sum(2); //total requests sorted by group (g1, g2)
static vector<int> type_sum(2); //wait report (due to group, due to occ. res.)


void *dbms_res_manager(void *t_id)
{	
	pthread_mutex_lock(&gsem);
		user_info* t_data = (user_info*)t_id;
		printf("User %i of group %i arrives to the DBMS.\n", t_data->usr_num, t_data->usr_grp);
		if(t_data->usr_grp == 1)
			req_sum[0]++;
		else
			req_sum[1]++;
		if(t_data->usr_grp!=cur_usr_grp) {
			printf("User %i is waiting due to its group.\n", t_data->usr_num);
			type_sum[0]++;
			pthread_cond_wait(&usr_grp_inc, &gsem); // block thread based on user group
		}
	pthread_mutex_unlock(&gsem);

	// aquire second mutex lock and check resources
	pthread_mutex_lock(&rsem);
		if(positions[(t_data->res_req)-1]!=0) { //block the thread on resource occupied
			type_sum[1]++;
			printf("User %i is waiting: Position %i of the database is being used by user %i.\n", t_data->usr_num, t_data->res_req, positions[(t_data->res_req)-1]);
			pthread_cond_wait(&res_conds[(t_data->res_req)-1], &rsem); 
		}
		if(positions[(t_data->res_req)-1]==0) {
			positions[(t_data->res_req)-1]=(t_data->usr_num); // set resource location equal to user occupying its position
			printf("User %i is accessing position %i of the database for %i seconds.\n", t_data->usr_num, t_data->res_req, t_data->req_t);
		}
	pthread_mutex_unlock(&rsem);

	sleep(t_data->req_t);

	pthread_mutex_lock(&rsem);
		positions[(t_data->res_req)-1]=0;
		printf("User %i finished its execution.\n", t_data->usr_num);
		pthread_cond_signal(&res_conds[(t_data->res_req)-1]);
	pthread_mutex_unlock(&rsem);

	pthread_mutex_lock(&gsem);
		grp_stager[cur_usr_grp-1].pop();
		if(grp_stager[cur_usr_grp-1].empty() && num_grp_cyles==1) {
			printf("All users from group %i finished their execution.\n", cur_usr_grp);
			if(cur_usr_grp==2)
				cur_usr_grp--;
			else
				cur_usr_grp++;
			printf("The users from group %i start their execution.\n", cur_usr_grp);
			num_grp_cyles++;
			pthread_cond_broadcast(&usr_grp_inc);
		}
		else if(grp_stager[cur_usr_grp-1].empty() && num_grp_cyles==2) { //print summary
			printf("Total requests: \n\t Group 1: %i \n\t Group 2: %i \n", req_sum[0], req_sum[1] );
			printf("Requests that waited:\n\t Due to its group: %i \n\t Due to a locked position: %i \n", type_sum[0], type_sum[1]);
		}
	pthread_mutex_unlock(&gsem);
	return NULL;
}

int main(int argc,  char *argv[]) {
	int num_t=0, // starts at zero and increments as each line is read in
	n_res=0; //number of resources (10 default by assignment spec)
	string line;

	/* Read in data into list vector, sorted by group number */
	cin >> cur_usr_grp;
	cin.ignore();
	while(getline(cin,line)) {
		stringstream linestream(line);
		user_reqs.push_back(user_info());
		user_reqs[num_t].usr_num = (num_t+1); //user number
		linestream >> user_reqs[num_t].usr_grp // group number
				   >> user_reqs[num_t].res_req // resource request
				   >> user_reqs[num_t].init_t // initial time request
				   >> user_reqs[num_t].req_t; // time required
		num_t++;
	}
	pthread_t tid[num_t];
	pthread_mutex_init(&gsem, NULL); // Initialize access to 1
	pthread_mutex_init(&rsem, NULL); // Initialize access to 1
	
	//for(int i=0; i<10;i++) {
	//	static pthread_cond_t res_cond=PTHREAD_COND_INITIALIZER;
	//	res_conds.push_back(res_cond);
	//}

	int timer=0;
	grp_stager.push_back(queue<int>());
	grp_stager.push_back(queue<int>());
	/* Main thread by default spawns other threads from t_requests, depending on their group.*/
   	for(int i=0; i < num_t; i++) {
   		if(user_reqs[i].usr_grp==1)
   			grp_stager[0].push(user_reqs[i].usr_num);
   		else
   			grp_stager[1].push(user_reqs[i].usr_num);
   		if(pthread_create(&tid[i], NULL, dbms_res_manager,(void *) &user_reqs[i])) {
			fprintf(stderr, "User query could not be dispatched by the server.\n");
			return 1;
		}
		sleep(user_reqs[i].init_t); // sleep for the initial request time
   			
   	}
   	for (int i = 0; i < num_t; i++)
        pthread_join(tid[i], NULL);
   	return 0;

}
