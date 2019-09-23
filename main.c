#include "gpiolib_addr.h"
#include "gpiolib_reg.h"

#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdio.h>				//for the printf() function
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#include <unistd.h> 			//needed for sleep
#include <sys/ioctl.h> 			//needed for the ioctl function
#include <stdlib.h> 			//for atoi
#include <time.h> 				//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()

//Macro to print messages onto a file
//Passes in file name, current time, program name, and message
//Outputs message onto the given file
#define PRINT_MSG(file, time, programName, str) \
	do{ \
			fprintf(file, "%s : %s : %s", time, programName, str); \
			fflush(file); \
	}while(0)

//This Macro will be used to see if the log file and stats file have the correct directory
#define DIRECTORY "/home/pi"

//Define the following Macros for string comparison
//when reading the config file in the 'readConfig' function
#define TIMEOUT "WATCHDOG_TIMEOUT"
#define LOGFILE "LOGFILE"
#define STATSFILE "STATSFILE"

//This function will read the config value to obtain the following:
//Watchdog timeout value, name of log file, and name of stats file
void readConfig(FILE* configfile, int* timeout, char* logfilename, char* statsfilename)
{

	//Create a buffer array for configuration file contents
	char buffer[500];

	//Assign timeout to be 0 for calculations
	*timeout = 0;

	//These will be character counters for arrays for logfilename and statsfilename
	int logCounter = 0;
	int statsCounter = 0;

	//Character array 'evaluate' will store the string before every equal sign encountered.
	//This array will then be used to assess which variable is being altered.
	char evaluate[50];

	//evalCounter will be character counter for the evaluate string
	int evalCounter = 0;

	typedef enum{START, NEW_LINE, GOT_HASH, GOT_CHAR, GOT_EQUAL, TIME_OUT, LOG_FILE, STATS_FILE, DONE}State;

	//Initialize CONFIG_STATE to START
	State CONFIG_STATE = START;

	//Assign all contents of the configfile into the buffer array
	//Exits loop once hits the end of the config file
	//This is done so because in this function, the contents of the config file will
	//be evaluated character by character
	for(int i = 0; !feof(configfile); i++)
	{
		buffer[i] = fgetc(configfile);
	}

	//Following for loop will change values for timeout, logfilename, and statsfilename
	//Increment i at the end of loop
	//Exit loop after reached the DONE state
	//Refer to Fig. 1 for corresponding State Machine
	for(int i = 0; buffer[i] != 0; i++)
	{
		switch(CONFIG_STATE)
		{
			case START:
				if(buffer[i] == '#')
				{
					CONFIG_STATE = GOT_HASH;
				}
				else if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;
				}
				else if(buffer[i] != 0)
				{
					CONFIG_STATE = GOT_CHAR;

					//Decrement i here to have buffer[i] remain at the character directly after the new line 
					//for the next iteration through the switch statement
					i--;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;
				}
				break;

			case NEW_LINE:
				if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;
				}
				else if(buffer[i] == '#')
				{
					CONFIG_STATE = GOT_HASH;
				}
				else if(buffer[i] != 0)
				{
					CONFIG_STATE = GOT_CHAR;

					//Decrement i here to have buffer[i] remain at the character directly after the new line 
					//for the next iteration through the switch statement
					i--;
				}
				else if(buffer[i] == 0)
				{					
					CONFIG_STATE = DONE;
				}
				break;

			case GOT_HASH:
				if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;
				}
				else if(buffer[i] != 0)
				{
					CONFIG_STATE = GOT_HASH;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;
				}
				break;

			case GOT_CHAR:
				if(buffer[i] == '=')
				{
					CONFIG_STATE = GOT_EQUAL;

					//Assign NULL value to end of evaluate string once all characters 
					//before the equals sign have been evaluated
					//This will represent the end of the array
					evaluate[evalCounter] = 0;
				}
				else if(buffer[i] != 0)
				{
					CONFIG_STATE = GOT_CHAR;

					//Store whatever is in the config file to the evaluate array
					evaluate[evalCounter] = buffer[i];

					//Increment the character counter for the evaluate array
					evalCounter++;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;
				}
				break;

			case GOT_EQUAL:

				//The following will evaluate which variable is currently being
				//read from the buffer array
				if(strcmp(evaluate, TIMEOUT) == 0)
				{
					CONFIG_STATE = TIME_OUT;
				}
				else if(strcmp(evaluate, LOGFILE) == 0)
				{
					CONFIG_STATE = LOG_FILE;
				}
				else if(strcmp(evaluate, STATSFILE) == 0)
				{
					CONFIG_STATE = STATS_FILE;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;
				}
				else
				{
					CONFIG_STATE = DONE;

					//If config file has been written improperly, assign garbage values to the variables
					//Once evaluated by other functions, then the default values will be used instead
					*timeout = -1;
					strcpy(logfilename, "\0");
					strcpy(statsfilename, "\0");
				}
				
				//Decrement i here to have buffer[i] remain at the character directly after the new line 
				//for the next iteration through the switch statement
				i--;

				//Reset the evalCounter for the next evaluation
				evalCounter = 0;
				
				break;

			case TIME_OUT:
				if(buffer[i] >= '0' && buffer[i] <= '9')
				{
					CONFIG_STATE = TIME_OUT;

					//Assign numerical value of buffer[i] to *timeout
					*timeout = (*timeout * 10) + (buffer[i] - '0');
				}
				else if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;
				}
				break;

			case LOG_FILE:
				if(buffer[i] != 0 && buffer[i] != '\n')
				{
					CONFIG_STATE = LOG_FILE;

					//Assign character in buffer[i] to logfilename[logCounter]
					logfilename[logCounter] = buffer[i];

					//Increment logCounter to get next array index in logfilename for the next character
					logCounter++;
				}
				else if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;

					//Assign NULL value to the end of logfilename once all characters evaluated
					//This represents the end of the array
					logfilename[logCounter] = 0;
				}
				else if(buffer[i] == 0)
				{
					CONFIG_STATE = DONE;

					//Assign NULL value to end of logfilename if file has ended here
					//This represents the end of the array
					logfilename[logCounter] = 0;
				}
				break;

			case STATS_FILE:
				if(buffer[i] != 0 && buffer[i] != '\n')
				{
					CONFIG_STATE = STATS_FILE;

					//Assign character in buffer[i] to statsfilename[statsCounter]
					statsfilename[statsCounter] = buffer[i];

					//Increment statsCounter to get next array index in statsfilename for the next character
					statsCounter++;
				}
				else if(buffer[i] == '\n')
				{
					CONFIG_STATE = NEW_LINE;

					//Assign NULL value to the end of the statsfilename string once all characters evaluated
					//This represents the end of the array
					statsfilename[statsCounter] = 0;
				}
				else if(buffer[i] == 0)
				{
					//Since this is the last variable to be evaluated, hence only can go to the DONE state
					CONFIG_STATE = DONE;

					//Assign NULL value to the end of statsfilename string if file ends here
					//This represents the end of the array
					statsfilename[statsCounter] = '\0';
				}
				break;

			case DONE:
				break;

			default:
				break;
		}	
	}
}

//This function will get the current time using the gettimeofday function
void getTime(char* buffer)
{
	//Create a timeval struct named tv
  	struct timeval tv;

	//Create a time_t variable named curtime
  	time_t curtime;

	//Get the current time and store it in the tv struct
  	gettimeofday(&tv, NULL); 

	//Set curtime to be equal to the number of seconds in tv
  	curtime=tv.tv_sec;

	//This will set buffer to be equal to a string that in
	//equivalent to the current date, in a month, day, year and
	//the current time in 24 hour notation.
  	strftime(buffer,30,"%m-%d-%Y  %T.\0",localtime(&curtime));
}

//This function will output messages to the stats file
void outputStats(FILE* statsFile, int laser1Count, int laser2Count, int numberIn, int numberOut, char* Time, char* programName)
{
	//Initialize the following to store strings that will be printed onto the stats file
	char strLaser1Count[50];
	char strLaser2Count[50];
	char strNumberIn[50];
	char strNumberOut[50];

	//Using the sprintf function, convert laser1Count, laser2Count, numberIn and numberOut from
	//int variables to strings. This is done so that the final values can be passed into the
	//PRINT_MSG function and printed onto the stats file properly.
	sprintf(strLaser1Count, "Laser 1 was broken %d times\n\n", laser1Count);
	sprintf(strLaser2Count, "Laser 2 was broken %d times\n\n", laser2Count);
	sprintf(strNumberIn, "%d objects entered the room\n\n", numberIn);
	sprintf(strNumberOut, "%d objects exitted the room\n\n", numberOut);

	//Get current time
	getTime(Time);

	//Print to the statsfile all of the above defined strings in sequential order
	PRINT_MSG(statsFile, Time, programName, strLaser1Count);
	PRINT_MSG(statsFile, Time, programName, strLaser2Count);
	PRINT_MSG(statsFile, Time, programName, strNumberIn);
	PRINT_MSG(statsFile, Time, programName, strNumberOut);
}

//This function will check to see if the configuration file has correctly configured the address of the log file
//If it hasn't, it will assign the default address for later use.
void checkLogFile(char* logFileName, char* Time, char* programName)
{
	//Initialize the following array to evaluate for log file directories
	char direcLogFile[30];

	//Assign the first 8 characters of logFileName (which is the directory) to direcLogFile
	for(int i = 0; i < 8; i++)
	{
		direcLogFile[i] = logFileName[i];
	}

	//Create FILE pointer to the default log file and set to overwrite
	FILE* defLogFile = fopen("/home/pi/Lab4Default.log", "w");

	//Get current time
	getTime(Time);

	//Check to see if the log file directory is valid or not
	if(strcmp(direcLogFile, DIRECTORY) != 0)
	{
		//Copy default address to logfilename again
		strcpy(logFileName, "/home/pi/Lab4Default.log");

		//Initialize a file pointer 'logFile' to point to the determined log file.
		//Set to overwrite the file so that any previous text would be erased before logging any information
		//Fopen used so that if file does not exist, it will be created
		FILE* logFile = fopen(logFileName, "w");
		
		//If the timeout value is invalid, output a message to the log file
		PRINT_MSG(logFile, Time, programName, "The log file directory is invalid: default log file has been opened.\n\n");

		//Before continuing, close the logFile
		fclose(logFile);
	}
	else
	{
		//Initialize a file pointer 'logFile' to point to the determined log file.
		//Set to overwrite the file so that any previous text would be erased before logging any information
		//Fopen used so that if file does not exist, it will be created
		FILE* logFile = fopen(logFileName, "w");

		//Output an error message if the log file cannot be read
		if(!logFile)
		{
			//If file cannot be read, close it
			fclose(logFile);

			//Copy default address to logfilename again
			strcpy(logFileName, "/home/pi/Lab4Default.log");

			//Initialize a file pointer 'logFile' to point to the determined log file.
			//Set to overwrite the file so that any previous text would be erased before logging any information
			//Fopen used so that if file does not exist, it will be created
			FILE* logFile = fopen(logFileName, "w");
			
			//If the timeout value is invalid, output a message to the log file
			PRINT_MSG(logFile, Time, programName, "The log file cannot be opened: default log file has been opened.\n\n");
		}
		else
		{
			//If the log file can be read, output a message to the log file
			PRINT_MSG(logFile, Time, programName, "The log file has been opened.\n\n");

			//If the logFile is not the default log file, then also print the message onto the default log file
			//(this will avoid printing onto the default log file twice)
			if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
			{
				PRINT_MSG(defLogFile, Time, programName, "The configured log file has been opened.\n\n");
			}
		}

		//Before continuing, close the logFile
		fclose(logFile);
	}

	//Before continuing, close the defLogFile
	fclose(defLogFile);
}

//This function will check to see if the configuration file has correctly configured the address of the stats file
//If it hasn't, it will assign the default address for later use.
void checkStatsFile(FILE* logFile, char* logFileName, char* statsFileName, char* Time, char* programName)
{

	//Initialize the following array to evaluate for stats file directories
	char direcStatsFile[30];

	//Assign the first 8 characters of statsFileName (which is the directory) to direcStatsFile
	for(int i = 0; i < 8; i++)
	{
		direcStatsFile[i] = statsFileName[i];
	}

	//Create FILE pointer that will be used to open and append to the default log file
	FILE* defLogFile = fopen("/home/pi/Lab4Default.log", "a");

	//Get current time
	getTime(Time);

	//Check to see if the stats file directory is valid or not
	if(strcmp(direcStatsFile, DIRECTORY) != 0)
	{
		//Copy default address to statsfilename again
		strcpy(statsFileName, "/home/pi/Lab4Default.stats");

		//Initialize a file pointer 'statsFile' to point to the determined stats file. 
		//Set to overwrite the file so that any previous text would be erased before printing any information into the file
		//Fopen used so that if file does not exist, it will be created
		FILE* statsFile = fopen(statsFileName, "w");
		
		//If the stats file directory is invalid, output a message to the log file
		PRINT_MSG(logFile, Time, programName, "Stats file directory is invalid: default stats file has been opened instead.\n\n");

		//If the logFile is not the default log file, then also print the message onto the default log file
		//(this will avoid printing onto the default log file twice)
		if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
		{
			PRINT_MSG(defLogFile, Time, programName, "Stats file directory is invalid: default stats file has been opened instead.\n\n");
		}

		//Before continuing, close the statsFile
		fclose(statsFile);
	}
	else
	{
		//Initialize a file pointer 'statsFile' to point to the determined log file.
		//Set to overwrite the file so that any previous text would be erased before logging any information
		//Fopen used so that if file does not exist, it will be created
		FILE* statsFile = fopen(statsFileName, "w");

		//Output an error message if the stats file cannot be read
		if(!statsFile)
		{
			//If file cannot be read, close it
			fclose(statsFile);

			//Copy default address to statsfilename again
			strcpy(statsFileName, "/home/pi/Lab4Default.stats");

			//Initialize a file pointer 'statsFile' to point to the determined stats file.
			//Set to overwrite the file so that any previous text would be erased before logging any information
			//Fopen used so that if file does not exist, it will be created
			FILE* statsFile = fopen(statsFileName, "w");
			
			//If the timeout value is invalid, output a message to the log file
			PRINT_MSG(logFile, Time, programName, "Stats file cannot be opened: default stats file has been opened instead.\n\n");

			//If the logFile is not the default log file, then also print the message onto the default log file
			//(this will avoid printing onto the default log file twice)
			if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
			{
				PRINT_MSG(defLogFile, Time, programName, "Stats file cannot be opened: default stats file has been opened instead.\n\n");
			}
		}
		else
		{
			//If the stats file can be read, output a message to the log file
			PRINT_MSG(logFile, Time, programName, "The configured stats file has been opened.\n\n");

			//If the logFile is not the default log file, then also print the message onto the default log file
			//(this will avoid printing onto the default log file twice)
			if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
			{
				PRINT_MSG(defLogFile, Time, programName, "The configured stats file has been opened.\n\n");
			}
		}
		//Before continuing, close the statsFile
		fclose(statsFile);
	}
	//Before continuing, close the defLogFile
	fclose(defLogFile);
}

//This function will check if the timeOut configured is within limits
//If not, it will assign the default value of 10 seconds
void checkTimeOut(FILE* logFile, char* logFileName, int* timeOut, char* Time, char* programName)
{
	//Create FILE pointer that will be used to open and append to the default log file
	FILE* defLogFile = fopen("/home/pi/Lab4Default.log", "a");

	//Get current time
	getTime(Time);

	//Check to see that the timeout values are within bounds
	if(*timeOut > 15 || *timeOut <= 0)
	{
		//Assign default value to timeout again
		*timeOut = 10;
		
		//If the timeout value is invalid, output a message to the log file
		PRINT_MSG(logFile, Time, programName, "The timeout value is invalid: will use default value instead.\n\n");

		//If the logFile is not the default log file, then also print the message onto the default log file
		//(this will avoid printing onto the default log file twice)
		if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
		{
			PRINT_MSG(defLogFile, Time, programName, "The timeout value is invalid: will use default value instead.\n\n");
		}
	}
	else
	{
		//If the timeout value is valid, output a message to the log file
		PRINT_MSG(logFile, Time, programName, "The configured timeout value is valid.\n\n");

		//If the logFile is not the default log file, then also print the message onto the default log file
		//(this will avoid printing onto the default log file twice)
		if(strcmp(logFileName, "/home/pi/Lab4Default.log") != 0)
		{
			PRINT_MSG(defLogFile, Time, programName, "The configured timeout value is valid.\n\n");
		}
	}
	//Before continuing, close the defLogFile
	fclose(defLogFile);
}

//This function should initialize the GPIO pins
GPIO_Handle initializeGPIO()
{
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if(gpio == NULL)
	{
		perror("Could not initialize GPIO");
	}
	return gpio;
}

//This is a helper function used in the laserDiodeStatus function.
//It passes in the number photodiode being evaluated (1 or 2)
//It returns the value of the gpio pin corresponding to the photodiode number plugged into it
int pinNumberPhotoDiode(int diodeNumber)
{
	if(diodeNumber == 1)
	{
		//17 represents that GPIO pin #17 on the Pi is connected to photodiode #1
		return 17;
	}
	else if(diodeNumber == 2)
	{
		//27 represents that GPIO pin #27 on the Pi is connected to photodiode #2
		return 27;
	}
	else
	{
		//If neither 1 nor 2 is passed into the function, then it is an invalid LED number
		return -1;
	}
}

//This function accepts the photodiode number (1 or 2) and outputs
// 0 if the laser beam is not reaching the diode, 1 if the laser
//beam is reaching the diode or -1 if an error occurs.
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber)
{
	//Returns -1 if the gpio is invalid
	if(gpio == NULL)
	{
		return -1;
	}

	//Uses helper function 'pinNumberPhotoDiode' to find the GPIO pin number being evaluated
	int PIN_NUM = pinNumberPhotoDiode(diodeNumber);

	if(diodeNumber == 1 || diodeNumber == 2)
	{
		//Create an unsigned int of size 32 bits 'level-reg' and assigns value of the photodiode at this moment
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		//Returns 1 if laser beam reaches the photodiode
		//Returns 0 if laser beam does not reach the photodiode
		if(level_reg & (1 << PIN_NUM))
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	//Returns -1 if the diodeNumber is neither 1 or 2 (i.e. an error has occured)
	else
	{
		return -1;
	}
}

int main(const int argc, const char* const argv[])
{

	//Create a character array that points to the command line
	const char* argName = argv[0];

	//Create variable to determine the length of the program's name
	int nameLength = 0;

	//Find the length of the program name without the /home/pi/ at the start of argv[0]
	for(int i = 0; argName[i+9] != 0; i++)
	{
		nameLength++;
	}

	//Create an array to store the program name
	char programName[nameLength];

	//Copy the name of the program without the "/home/pi/"" at the start of argv[0]
	for(int i = 0; i <= nameLength; i++)
	{
		//Assign the 'i+1'th value from argName to programName
		//This avoids putting the period into the programName array
		programName[i] = argName[i + 9];

		//Assign the NULL terminator to the index directly after the last character of the programName
		programName[i+1] = '\0';

	}

	//Initialize a file pointer 'configFile' to point to Lab4.cfg. Set to read the file.
	FILE* configFile = fopen("/home/pi/Lab4.cfg", "r");

	//Output an error message if Lab4.cfg cannot be read
	if(!configFile)
	{
		perror("The config file could not be opened");
		return -1;
	}

	//Create watchdog time out (timeOut), name of log file (logFileName), and name of stats file (statsFileName)
	//Set all these variables to their default values
	int timeOut = 10;
	char logFileName[50] = "/home/pi/Lab4Default.log\0";
	char statsFileName[50] = "/home/pi/Lab4Default.stats\0";

	//Read the config file and assign values to timeOut, logFileName and statsFileName found in the config file
	readConfig(configFile, &timeOut, logFileName, statsFileName);

	//Close the config file
	fclose(configFile);

	//Create a character array to hold the current time
	char Time[30];

	//Call the function to check if the log file has been correctly configured
	//If not, it will assign a default address to logFileName
	checkLogFile(logFileName, Time, programName);

	//Before proceeding, we only want to append to the log file
	FILE* logFile = fopen(logFileName, "a");

	//Call the function to check if the stats file has been correctly configured
	//If not, it will assign a default address to statsFileName
	checkStatsFile(logFile, logFileName, statsFileName, Time, programName);

	//Before proceeding, we only want to append to the stats file
	FILE* statsFile = fopen(statsFileName, "a");

	//Call the function to check if the timeout value is valid or not
	//If not, it will assign the default value of 10 secs to timeout
	checkTimeOut(logFile, logFileName, &timeOut, Time, programName);

	//Get current time
	getTime(Time);

	//Initialize the gpio pins by calling the initializeGPIO function
	GPIO_Handle gpio = initializeGPIO();

	//Output an error message on screen & in the log file if the GPIO pins cannot be initialized
	if(gpio == NULL)
	{
		PRINT_MSG(logFile, Time, programName, "The GPIO pins could not be initialized.\n\n");
		perror("The GPIO pins could not be initialized.");
		return -1;
	}

	//If the GPIO pins have been initialized, print a message to the log file
	PRINT_MSG(logFile, Time, programName, "The GPIO pins have been initialized.\n\n");

	//Get current time
	getTime(Time);

	//Create variable 'watchdog' used to access the /dev/watchdog file
	int watchdog;

	//Use the open function here to open the /dev/watchdog file
	//If it does not open (i.e. returns a negative value), then output an error message onto the log file & screen
	if((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0)
	{
		PRINT_MSG(logFile, Time, programName, "The watchdog device could not be opened.\n\n");
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		return -1;
	}
	
	//Get current time
	getTime(Time);
	
	//If the watchdog can be opened, output a message to the log file
	PRINT_MSG(logFile, Time, programName, "The Watchdog file has been opened\n\n");

	//Use ioctl function to set the time limit of the watchdog timer to 15 seconds
	//The time limit cannot be set higher than 15 seconds
	//If it is, then it will reject that value and use the previously set time limit
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeOut);
	
	//Get current time
	getTime(Time);

	//Output a message to the log file that the time limit has been set
	PRINT_MSG(logFile, Time, programName, "The Watchdog time limit has been set\n\n");

	//Change watchdog timer value to value of timeOut as read in the config file
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeOut);

	//Create the states that will be used in the state machine
	typedef enum{START, ONLY_LASER1_BROKEN, ONLY_LASER2_BROKEN, BOTH_BROKEN, BOTH_UNBROKEN, DONE}State;

	//Initialize LASER_STATE to START
	State LASER_STATE = START;

	//Following variables will be used in the switch statement to determine whether or not laser1 or laser2 has
	//already previously broken.
	int laser1HasBroken = 0;
	int laser2HasBroken = 0;

	//Following variables will be used for the outputStats function defined above
	int laser1Count = 0;
	int laser2Count = 0;
	int numberIn = 0;
	int numberOut = 0;

	//Output statistics to stats file for the initial count
	outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);

	//Initialize a variable representing the total number of CPU clock ticks within the watchdog timeout
	int totalTicks = CLOCKS_PER_SEC*timeOut;

	//Calculate a fifteenth of the totalTicks
	//This will be used to determine when to kick the watchdog
	//(i.e. the watchdog will be kicked every fifteenth of the timeOut)
	long int ticksInterval = totalTicks/15;

	//Initialize a constant to represent the time taken from the beginning of the program till now
	const clock_t ticksInitial = clock();

	//Calculate the amount of ticks elapsed since ticksInitial
	clock_t ticksElapsed = clock() - ticksInitial;

	//Continue in while loop indefinitely (so long as the watchdog is kicked)
	//Exit the loop only if the program is forced to terminate
	while(1)
	{

		//See Fig. 2 for the corresponding state machine
		switch(LASER_STATE)
		{

			case START:
				//The following restriction was made here:
				//The program must begin with both lasers unbroken
				
				if(laserDiodeStatus(gpio, 1) == 1 && laserDiodeStatus(gpio, 2) == 1)
				{
					LASER_STATE = BOTH_UNBROKEN;

					//Get current time
					getTime(Time);

					//If both lasers unbroken, output a message to the log file that the program has started
					PRINT_MSG(logFile, Time, programName, "Both lasers unbroken: program successfully started.\n\n");
				}
				else
				{
					//If both lasers begin as not unbroken, then exit the program and output an error message to the screen & log file
					
					//Get current time
					getTime(Time);

					//Print a message to the screen to notify the user why the program has not started
					perror("Must start with both lasers unbroken: exiting program.\n");

					//Output a message to the log file that the program has unsuccessfully started
					PRINT_MSG(logFile, Time, programName, "Must start with both lasers unbroken: exiting program.\n\n");
					
					//Write 'V' to the watchdog file to disable it
					write(watchdog, "V", 1);
					
					//Get current time
					getTime(Time);
					
					//Log that the watchdog was disabled
					PRINT_MSG(logFile, Time, programName, "The watchdog was disabled. \n\n");

					//Close the watchdog file
					close(watchdog);

					//Get current time
					getTime(Time);

					//Log that the watchdog was closed
					PRINT_MSG(logFile, Time, programName, "The watchdog was closed. \n\n");

					//Free the gpio pins
					gpiolib_free_gpio(gpio);

					//Get current time
					getTime(Time);

					//Log that the GPIO pins are freed
					PRINT_MSG(logFile, Time, programName, "The GPIO pins have been freed. \n\n");

					//Return negative value to indicate an error has occured
					return -1;
				}
				break;

			case BOTH_UNBROKEN:

				//Once in this state, reassign 0 to the following variables to indicate that both lasers are now unbroken again
				laser1HasBroken = 0;
				laser2HasBroken = 0;

				if(laserDiodeStatus(gpio, 1) == 1 && laserDiodeStatus(gpio, 2) ==1)
				{
					LASER_STATE = BOTH_UNBROKEN;
				}
				else if(laserDiodeStatus(gpio, 1)== 1 && laserDiodeStatus(gpio, 2)==0)
				{
					LASER_STATE = ONLY_LASER2_BROKEN;

					//If only laser 2 has broken, increment the number of times laser 2 has broken
					laser2Count++;

					//Assign 1 to laser2HasBroken to indicate that laser 2 has been broken now
					laser2HasBroken = 1;

					//Get current time
					getTime(Time);

					//Output message into log file that laser 2 has been broken
					PRINT_MSG(logFile, Time, programName, "Laser 2 has been broken.\n\n");

					//Output statistics to stats file to update counts
					outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);

				}
				else if(laserDiodeStatus(gpio, 1)== 0 && laserDiodeStatus(gpio, 2)==1)
				{					
					LASER_STATE = ONLY_LASER1_BROKEN;

					//If only laser 1 has broken, increment the number of times laser 1 has broken
					laser1Count++;

					//Assign 1 to laser1HasBroken to indicate that laser 1 has been broken now
					laser1HasBroken = 1;

					//Get current time
					getTime(Time);

					//Output message into log file that laser 1 has been broken
					PRINT_MSG(logFile, Time, programName, "Laser 1 has been broken.\n\n");

					//Output statistics to stats file to update counts
					outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);
				}
				break;

			case ONLY_LASER1_BROKEN:
				if(laserDiodeStatus(gpio, 1) == 0 && laserDiodeStatus(gpio, 2) == 1)
				{
					LASER_STATE = ONLY_LASER1_BROKEN;
				}
				else if(laserDiodeStatus(gpio, 1) == 1 && laserDiodeStatus(gpio, 2) == 1)
				{
					LASER_STATE = BOTH_UNBROKEN;

					//Get current time
					getTime(Time);

					//Output message into log file that both lasers are now unbroken
					PRINT_MSG(logFile, Time, programName, "Both lasers are now unbroken.\n\n");

					//If laser2 has been previously broken, then laser 1 is broken and now both lasers are unbroken
					//Then, an object has exitted the room
					if(laser2HasBroken)
					{
						//Increment number of objects exitting the room
						numberOut++;
						
						//Get current time
						getTime(Time);

						//Output message into log file that an object has exitted the room
						PRINT_MSG(logFile, Time, programName, "An object has exitted the room.\n\n");

						//Output statistics to stats file to update counts
						outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);
					}
				}
				else if(laserDiodeStatus(gpio, 1) == 0 && laserDiodeStatus(gpio, 2) == 0)
				{
					LASER_STATE = BOTH_BROKEN;

					//Increment number of times laser 2 is broken
					laser2Count++;

					//Get current time
					getTime(Time);

					//Output message into log file that both lasers are broken
					PRINT_MSG(logFile, Time, programName, "Both lasers are now broken.\n\n");

					//Output statistics to stats file to update counts
					outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);
				}
				break;

			case ONLY_LASER2_BROKEN:
				if(laserDiodeStatus(gpio, 2) == 0 && laserDiodeStatus(gpio, 1) == 1)
				{
					LASER_STATE = ONLY_LASER2_BROKEN;
				}
				else if(laserDiodeStatus(gpio, 1) == 1 && laserDiodeStatus(gpio, 2) == 1)
				{
					LASER_STATE = BOTH_UNBROKEN;

					//Get current time
					getTime(Time);

					//Output message into log file that both lasers are now unbroken
					PRINT_MSG(logFile, Time, programName, "Both lasers are now unbroken.\n\n");

					//If laser1 has been previously broken, then laser 2 is broken and now both lasers are unbroken
					//Then, an object has entered the room
					if(laser1HasBroken)
					{
						//Increment number of objects entering the room
						numberIn++;

						//Get current time
						getTime(Time);

						//Output message into log file that an object has entered the room
						PRINT_MSG(logFile, Time, programName, "An object has entered the room.\n\n");

						//Output statistics to stats file to update counts
						outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);
					}
				}
				else if(laserDiodeStatus(gpio, 1) == 0 && laserDiodeStatus(gpio, 2) == 0)
				{
					LASER_STATE = BOTH_BROKEN;

					//Increment number of times laser 1 is broken
					laser1Count++;

					//Get current time
					getTime(Time);

					//Output message into log file that both lasers are broken
					PRINT_MSG(logFile, Time, programName, "Both lasers are now broken.\n\n");

					//Output statistics to stats file to update counts
					outputStats(statsFile, laser1Count, laser2Count, numberIn, numberOut, Time, programName);
				}
				break;

			case BOTH_BROKEN:
				if(laserDiodeStatus(gpio, 1)== 0 && laserDiodeStatus(gpio, 2)==0)
				{
					LASER_STATE = BOTH_BROKEN;
				}
				else if(laserDiodeStatus(gpio, 1)== 0 && laserDiodeStatus(gpio, 2)==1)
				{
					LASER_STATE = ONLY_LASER1_BROKEN;

					//Get current time
					getTime(Time);

					//Output message into log file that only laser 1 is broken after laser 2 has been unbroken
					PRINT_MSG(logFile, Time, programName, "Laser 2 has unbroken: only Laser 1 is now broken.\n\n");
				}
				else if(laserDiodeStatus(gpio, 1)== 1 && laserDiodeStatus(gpio, 2)==0)
				{
					LASER_STATE = ONLY_LASER2_BROKEN;

					//Get current time
					getTime(Time);

					//Output message into log file that only laser 2 is broken after laser 1 has been unbroken
					PRINT_MSG(logFile, Time, programName, "Laser 1 has unbroken: only Laser 2 is now broken.\n\n");
				}
				break;

			case DONE:	
				break;

			//If defaults, return -1 to indicate an error has occured
			default:
				return -1;
		}

		//Every time the ticksElapsed has reached a multiple of ticksInterval, enter the if statement
		//(i.e. every fifteenth of the timeout, enter the if statement)
		//This will keep the watchdog alive and print a message onto the log file
		if(ticksElapsed % ticksInterval == 0)
		{

			//Get current time
			getTime(Time);

			//Kick the watchdog
			ioctl(watchdog, WDIOC_KEEPALIVE, 0);

			//Print a message to the log file that the watchdog has been kicked
			PRINT_MSG(logFile, Time, programName, "The watchdog has been kicked.\n\n");
		}

		//Recalculate the ticksElapsed
		ticksElapsed = (clock() - ticksInitial);
	}
	return 0;
}
