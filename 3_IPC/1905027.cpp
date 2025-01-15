#include<bits/stdc++.h>
#include <pthread.h>
#include <semaphore.h>
#include <random>
#define PRINTING_STATION_COUNT 4
#define IDLE 1
#define QUEUE 2
#define PRINTING 3

using namespace std;

int studentNum, groupSize, w, x, y;

default_random_engine generator(1234);
pthread_mutex_t mutex_lock;
vector<sem_t> semPrint(200);
sem_t semBind;
sem_t semReadWrite;
int readCounter = 0;
int submissionCounter = 0;
long long int start_time;

class staffs
{
public:
    int staffID, read_delay, wait_delay;
    pthread_t thread;

    staffs(int staffID);
};

staffs::staffs(int staffID)
{
    this->staffID = staffID;
}

class students
{
public:
    int studentID, arrive_delay, print_delay, bind_delay, write_delay, printStationNum;
    int curState, isLeader;
    pthread_t thread;

    students(int studentID);
};

students::students(int studentID)
{
    this->studentID = studentID;
    isLeader = 0;
    curState = IDLE;
    printStationNum = (studentID) % PRINTING_STATION_COUNT + 1;
}

vector<students> student;
vector<staffs> staff;

int valueFromString(string str)
{
    int num = 0;
    int string_size = str.size();

    for (int i = 0; i < string_size; i++)   num = 10 * num + (str[i] - 48);
    //when the number of team is of multiple digit

    return num;
}

int breakDownLine(vector<string> v)
{
    string wordNumberJ; // finds out the j-th word in a line

    for (int i = 0; i < 2; i++)
    {
        int j = 0;
        istringstream s(v[i]);
        do {
            s >> wordNumberJ;

            if(i==0)
            {
                if(j==0)        studentNum = (valueFromString(wordNumberJ));
                else if(j==1)   groupSize = (valueFromString(wordNumberJ));
            }

            else
            {
                if(j==0)        w = (valueFromString(wordNumberJ));
                else if(j==1)   x = (valueFromString(wordNumberJ));
                else            y = (valueFromString(wordNumberJ));
            }
            j++;
        } while (s);
    }
}

void initializationStudent()
{
    int lead = 0;
    start_time = time(NULL);

	poisson_distribution<int> d_arrival(3), d_printing(w), d_binding(x), d_writing(y), d_waiting(3);

	for( int i = 1; i<= studentNum; i++)
    {
        students student_init(i);
		student.push_back(student_init);

        student[i-1].arrive_delay = d_arrival(generator);
        student[i-1].print_delay = d_printing(generator);
        student[i-1].bind_delay = d_binding(generator);
        student[i-1].write_delay = d_writing(generator);

        if( i % groupSize == 0) lead = 1;
        student[i-1].isLeader += lead;
    }
}

void initializationStaff()
{
	poisson_distribution<int> d_writing(y), d_waiting(3);
	for (int i = 1; i <= 2; i++)
	{
		staffs staff_init(i);
		staff.push_back(staff_init);

		staff[i-1].wait_delay = d_waiting(generator);
		staff[i-1].read_delay = d_writing(generator);
	}
}

void initializationSemaphore()
{
	pthread_mutex_init(&mutex_lock, 0);
	for (int i = 1; i <= studentNum; i++)
	{
		sem_init(&semPrint[i-1], 0, 0);
	}
	sem_init(&semBind, 0, 2);
	sem_init(&semReadWrite, 0, 1);
}

void assignPrinting(int id)
{
    int printingStationName = student[id-1].printStationNum;
    bool alreadyPrinting = false;

    for(int i=1; i<=studentNum; i++)
        if(student[i-1].printStationNum == printingStationName && student[i-1].curState == PRINTING)
        {
            alreadyPrinting = true;
            break;
        }

    if(student[id-1].curState == QUEUE && alreadyPrinting==false)
    {
        student[id-1].curState = PRINTING;
        sem_post(&semPrint[id-1]);
    }
}

void printingEntry(int idx)
{
    pthread_mutex_lock (&mutex_lock);
	student[idx-1].curState = QUEUE;
	assignPrinting(idx);
	pthread_mutex_unlock (&mutex_lock);
	sem_wait (&semPrint[idx-1]);
}

void printingExit(int id)
{
    vector<int> teamMates;
    vector<int> diffMembers;

    int groupNum = (id-1) / groupSize;
    int position = (id-1) % groupSize;

    int groupLast = groupNum * groupSize;
    int groupFirst = (groupNum-1) * groupSize;

    for(int i = 1; i<=studentNum; i++)
    {
        if(i>=groupFirst && i<=groupLast)   teamMates.push_back(i);
        else                                diffMembers.push_back(i);
    }

    vector<int>::iterator it;
    it = teamMates.begin();

    teamMates.erase(it+position);

    int teamMember = teamMates.size();
    int diffGroupMember = diffMembers.size();

    pthread_mutex_lock (&mutex_lock);
    for(int i= 0; i<teamMember; i++)    assignPrinting(teamMates[i]);
    for(int i= 0; i<diffGroupMember; i++)    assignPrinting(diffMembers[i]);
    pthread_mutex_unlock (&mutex_lock);
}

void reader( int id)
{
    pthread_mutex_lock(&mutex_lock);
    readCounter++;
    if(readCounter == 1)    sem_wait(&semReadWrite);
    pthread_mutex_unlock(&mutex_lock);

    sleep(staff[id-1].read_delay);

    pthread_mutex_lock(&mutex_lock);
    readCounter--;
    if(readCounter == 0)    sem_post(&semReadWrite);
    pthread_mutex_unlock(&mutex_lock);
}

void writer( int id)
{
    sem_wait(&semReadWrite);
    sleep(student[id-1].write_delay);
    sem_post(&semReadWrite);
}

void *student_func(void *arg)
{
    int id = *(int*) arg;

    sleep(student[id-1].arrive_delay);
    cout<<"Student "<<id<<" has arrived at the print station at time "<<time(NULL) - start_time<<endl;

    printingEntry(id);
    sleep(student[id-1].print_delay);
    printingExit(id);

    cout<<"Student "<<id<<" has finished printing at time "<<time(NULL) - start_time<<endl;

    if (student[id-1].isLeader == 1)
	{
		int others = groupSize - 1;
		for (int i = id - 1; others != 0; i--)
		{
			pthread_join(student[i-1].thread,NULL);
			others--;
		}

		int groupNum = ((id - 1) / groupSize) + 1;

		cout<<"Group "<< groupNum <<" has finished printing at time "<<time(NULL) - start_time<<endl;

		sem_wait(&semBind);
		cout<<"Group " << groupNum <<" has started binding at time " << time(NULL) - start_time <<endl;
		sleep(student[id-1].bind_delay);

		sem_post(&semBind);
		cout<< "Group " << groupNum <<" has finished binding at time "<< time(NULL) - start_time <<endl;

		writer(id);
		pthread_mutex_lock(&mutex_lock);
		submissionCounter++;
		pthread_mutex_unlock(&mutex_lock);
        cout<< "Group " << groupNum <<" has submitted report at time "<< time(NULL) - start_time <<endl;
	}

	pthread_exit(NULL);

	return nullptr;
}

void *staff_func(void *arg)
{
    int id = *(int *)arg;
    int totalSubmission = studentNum / groupSize;
    int randomNum = rand() % 4 + 1;

    while(1)
    {
        sleep(staff[id-1].wait_delay);
        cout<<"Staff "<<id<<" has started reading the entry book at time "<< time(NULL) - start_time <<endl;
        sleep(staff[id-1].read_delay);

        pthread_mutex_lock(&mutex_lock);
        if(submissionCounter == totalSubmission)
        {
            pthread_mutex_unlock(&mutex_lock);
            break;
        }
        pthread_mutex_unlock(&mutex_lock);


        staff[id-1].wait_delay = randomNum;
    }

    pthread_exit(NULL);
    return nullptr;
}

// Driver code
int main()
{
    /*
    vector<string> input(2, "");
    int lineNum = 0;

    // read from file.txt
    ifstream in("file1.txt");

    while(!in.eof())
    {
        // string to extract line from
        string text;

        // extracting line from file1.txt
        getline(in, text);
        input[lineNum] = text;
        lineNum++;
    }

    breakDownLine(input);
    */

    freopen("file1.txt", "r", stdin);


    cin >> studentNum >> groupSize >> w >> x >> y;

    vector<int> studentSerial;

    for( int i=1; i<=studentNum; i++)   studentSerial.push_back(i);

    random_shuffle(studentSerial.begin(), studentSerial.end());

    initializationStudent();
    initializationStaff();
    initializationSemaphore();

    pthread_t staff_thread[2];
    for( int i = 1; i<=studentNum; i++)
    {
        if(i<=2)    pthread_create(&staff_thread[i-1], NULL, staff_func, (void *)&staff[i-1].staffID);
        pthread_create(&(student[studentSerial[i-1]].thread), NULL, student_func, (void *)&studentSerial[i-1]);
    }

    for(int i=1; i<=studentNum; i++)
        if(student[i-1].isLeader==1)
            pthread_join(student[i-1].thread, NULL);

    sem_destroy(&semBind);
    sem_destroy(&semReadWrite);

    int semPrintSize = semPrint.size();

    for(int i=0; i < semPrintSize; i++)
        sem_destroy(&semPrint[i]);

    return 0;
}
