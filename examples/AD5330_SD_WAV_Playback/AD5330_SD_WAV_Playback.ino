/*
Playing WAV Files with a DAC Tutorial
Arduino Sketch Version of WAV Files from SD Card
SparkFun Electronics
Written by Ryan Owens
3/16/2010

Code Description: Uses an Arduino Duemillanove or Arduino Pro to play WAV files on the 
AD5330 DAC from an SD card. See this tutorial for more information on the code and how
to hook up the circuit:

Attributions: Special thanks to Roland Riegel for providing an open source FAT library 
for AVR microcontrollers. See more of his projects here:
http://www.roland-riegel.de/

This code is provided under the Creative Commons Attribution License. More information can be found here:
http://creativecommons.org/licenses/by/3.0/

(Use our code freely! Please just remember to give us credit where it's due. Thanks!)
*/

//Add libraries to support FAT16 on the SD Card.
//(Note: If you already have these libraries installed in the directory, they'll have to remove in order to compile this.)
#include <byteordering.h>
#include <fat.h>
#include <fat_config.h>
#include <partition.h>
#include <partition_config.h>
#include <sd-reader_config.h>
#include <sd_raw.h>
#include <sd_raw_config.h>

//This is the amount of data to be fetched from the SD card for each read.
#define BUFFERSIZE	256

//Define the pin connections from the Arduino to the AD5330
#define  CS  18
#define  WR  17
#define  LDAC  16
#define  CLR  15
#define  PD  10
#define  BUF  9
#define GAIN  14

//This struct will hold information related to the WAV file that's being read.
typedef struct 
{
  int format;
  int sample_rate;
  int bits_per_sample;
}wave_format;
wave_format wave_info;

volatile unsigned char note=0;		//Holds the current voltage value to be sent to the AD5330.

unsigned char header[44];			//Holds the WAV file header
unsigned char buffer1[BUFFERSIZE], buffer2[BUFFERSIZE];	//Two cycling buffers which contain the WAV data.
char file_name[30];					//WAV file name.

char play_buffer=0;					//Keeps track of which buffer is currently being used
char new_buffer_ready=0;			//Flag used by 'Loop' code to tell the Interrupt that new data is ready in the buffer.
volatile unsigned int byte_count=0;	//Keeps track of the number of bytes read from the current buffer.
volatile char need_new_data=0;		//Flag used by Interrupt to tell 'Loop' code that a buffer is empty and needs to be refilled.

struct fat_dir_struct* dd;			//FAT16 directory
struct fat_dir_entry_struct dir_entry;	//FAT16 directory entry (A.K.A. a file)

struct fat_fs_struct* fs;			//FAT16 File System
struct partition_struct* partition;	//FAT16 Partition

struct fat_file_struct * file_handle;	//FAT16 File Handle

void setup()
{
  //Pins D0-D8 go to parallel control pins on AD5330
  for(int pin=0; pin<8; pin++)
  {
    pinMode(pin, OUTPUT);
  } 
  pinMode(CS, OUTPUT);
  pinMode(WR, OUTPUT);
  pinMode(LDAC, OUTPUT);
  pinMode(CLR, OUTPUT);
  pinMode(PD, OUTPUT);
  pinMode(BUF, OUTPUT); 
  
  digitalWrite(PD, HIGH);  //Enable the AD5330
  digitalWrite(GAIN, LOW);  //Set Gain to 1
  digitalWrite(BUF, LOW);  //Don't buffer the input
  digitalWrite(CS, HIGH);  //Set the CS high by default
  digitalWrite(WR, HIGH);  //Set the WR pin high by default
  digitalWrite(CLR, HIGH);  //Make sure Clear pin is disabled
  digitalWrite(LDAC, LOW);
  
  //Clock in Gain and Buffer Values
  digitalWrite(CS, LOW);
  delayMicroseconds(10);
  digitalWrite(WR, LOW);
  delayMicroseconds(10);
  
  digitalWrite(CS, LOW);
  delayMicroseconds(10);
  digitalWrite(WR, LOW);
  delayMicroseconds(10); 
  
  //Serial.begin(9600); 
}

//This timer interrupt will run every 45uS. A 45uS period equals a frequency of 22.222kHz. This is frequency of the WAV
//files that will be played by this sketch.
ISR(TIMER1_COMPA_vect){
  cli();  //Disable interrupts
  
  //Check to see if we've read all of the data in the current buffer
  if(byte_count==BUFFERSIZE)
  {
    need_new_data=1;	//Set a flag to tell the 'loop' code to refill the current buffer.
    byte_count=0;		//Reset the count
	//Check to see if new data exists in the alternate buffer
	if(new_buffer_ready==1)
    {
	  //If new data is available, reassign the play buffer.
      if(play_buffer==0)play_buffer=1;
      else play_buffer=0;
    }
    else
    {
	  //If no new data is available then wait for it!
      sei();
      return;
    }
  }
  
  //Find out which buffer is being used, and get data from it.
  if(play_buffer==0)note=buffer1[byte_count];
  else note=buffer2[byte_count];
  
  //Increase the byte_count since we've taken the current data.
  byte_count +=1;
  	
  //Clock in the current value that was retrieved from the play buffer.
  digitalWrite(CS, LOW);
  digitalWrite(WR, LOW);
  PORTD = note;	//Writing 'note' to PORTD is a much faster way to perform a digitalWrite() to D0-D7.
  digitalWrite(WR, HIGH);
  digitalWrite(CS, HIGH);	
  	
  sei(); //Re-enable interrupts
}  

void loop()
{
  int bytes_read=0; //Keeps track of how many bytes are read when accessing a file on the SD card.
  
  //Init Timer 1
  //Used for 45uS Interrupt
  TCCR1A = 0;		//Set Timer to normal mode
  TCCR1B = 0x0A;	//Set Timer clock to 2 MHz. Clear timer on compare
  TIMSK1 = 0x02;	//Enable Timer 1 Compare A Interrupt;
  OCR1AH = 0X00;	//Count to 90 before triggering an interrupt. Counting to 90 with a 2MHz clock makes
  OCR1AL = 0x5A;    //the interrupt trigger at 22.222kHz
  
  init_filesystem();	//Initialize the FAT16 file system on the SD card.

  if(get_wav_filename(dd, file_name));	//Find the first WAV file on the SD card (must be in the root directory)
  else while(1);	//If a WAV file isn't found then the sketch is stopped here.
  
  //Open the file	
  file_handle=open_file_in_dir(fs, dd, file_name);
  //Read the header information. Alternate purpose is to get to the DATA offset of the file.
  read_wav_header(file_handle, header);
  //Set the initial play buffer, and grab the initial data from the SD card.
  play_buffer=0;
  bytes_read = fat_read_file(file_handle, buffer1, BUFFERSIZE);
  bytes_read = fat_read_file(file_handle, buffer2, BUFFERSIZE);
  //Enable interrupts to start the wav playback.
  sei();
  while(1){
    if(need_new_data==1)	//need_new_data flag is set by ISR to indicate a buffer is empty and should be refilled
    {
      need_new_data=0;	//Clear the flag.
      if(play_buffer==0)	//play_buffer indicates which buffer is now empty
      {
        //Get the next BUFFERSIZE bytes from the file.
        bytes_read = fat_read_file(file_handle, buffer1, BUFFERSIZE);
      }
      else
      {
        //Get the next BUFFERSIZE bytes from the file.
        bytes_read = fat_read_file(file_handle, buffer2, BUFFERSIZE);
      }
      new_buffer_ready=1;	//new_buffer_ready flag tells the ISR that the buffer has been filled.
			
      //If file_read returns 0 or -1 file is over. Find the next file!
      if(bytes_read<=0)
      {
        cli();	//Disable interrupts to stop playback.
        fat_close_file(file_handle);	//Close the current file
        //Find the next WAV file in the SD card
        fat_reset_dir(dd);	//Make sure we start searching from the beginning of the directory
        find_file_in_dir(fs, dd, file_name, &dir_entry);	//Navigate to the current file in the directory
        if(get_wav_filename(dd, file_name));
        else while(1);	//If we don't find another wav file, stop everything!
				
        //If we get here we've found another wav file. Open it!
        file_handle=open_file_in_dir(fs, dd, file_name);
        //Get the file header and load the initial song data.
        read_wav_header(file_handle, header);
        play_buffer=0;
        bytes_read = fat_read_file(file_handle, buffer1, BUFFERSIZE);
        bytes_read = fat_read_file(file_handle, buffer2, BUFFERSIZE);
        sei();	//Start playing the song
      }
    }
  }
  
}

uint8_t find_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name, struct fat_dir_entry_struct* dir_entry)
{
	fat_reset_dir(dd);	//Make sure to start from the beginning of the directory!
    while(fat_read_dir(dd, dir_entry))
    {
        if(strcmp(dir_entry->long_name, name) == 0)
        {
            //fat_reset_dir(dd);
            return 1;
        }
    }

    return 0;
}

struct fat_file_struct* open_file_in_dir(struct fat_fs_struct* fs, struct fat_dir_struct* dd, const char* name)
{
    struct fat_dir_entry_struct file_entry;
    if(!find_file_in_dir(fs, dd, name, &file_entry))
        return 0;

    return fat_open_file(fs, &file_entry);
}

char init_filesystem(void)
{
	//setup sd card slot 
	if(!sd_raw_init())
	{
		return 0;
	}

	//open first partition
	partition = partition_open(sd_raw_read,
									sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
									sd_raw_write,
									sd_raw_write_interval,
#else
									0,
									0,
#endif
									0
							   );

	if(!partition)
	{
		//If the partition did not open, assume the storage device
		//is a "superfloppy", i.e. has no MBR.
		partition = partition_open(sd_raw_read,
								   sd_raw_read_interval,
#if SD_RAW_WRITE_SUPPORT
								   sd_raw_write,
								   sd_raw_write_interval,
#else
								   0,
								   0,
#endif
								   -1
								  );
		if(!partition)
		{
			return 0;
		}
	}

	//Open file system
	fs = fat_open(partition);
	if(!fs)
	{
		return 0;
	}

	//Open root directory
	fat_get_dir_entry_of_path(fs, "/", &dir_entry);
	dd=fat_open_dir(fs, &dir_entry);
	
	if(!dd)
	{
		return 0;
	}
	return 1;
}

char get_wav_filename(struct fat_dir_struct* cur_dir, char * new_file)
{
	//'directory' is a global variable of type directory_entry_struct

	//Get the next file from the root directory
	while(fat_read_dir(cur_dir, &dir_entry))
	{
		//If we found a .wav file, copy the name and exit!
		if(strcmp(&dir_entry.long_name[strlen(dir_entry.long_name)-4], ".wav")==0)
		{
			sprintf(new_file, "%s", dir_entry.long_name);
			return 1;
		}
	}
	
	return 0;	//If we got to the end of the current directory without finding a wav file, return 0
}

void read_wav_header(struct fat_file_struct* file, unsigned char * header)
{
	char field[2];

	fat_read_file(file, header, 44);	//Read the wav file header
	//Extract the Sample Rate field from the header
	sprintf(field, "%x%x", header[25], header[24]);
	str2int(field, &wave_info.sample_rate);
	//Extract the audio format from the header
	sprintf(field, "%x%x", header[21], header[20]);
	str2int(field, &wave_info.format);	
	//Extract the bits per sample from the header
	sprintf(field, "%x%x", header[35], header[34]);
	str2int(field, &wave_info.bits_per_sample);	
	return;
}

/* convert a hex number string into int */
int str2int(char * str,int * val_p)
{
  int i, len;
  char c;

  *val_p = 0;
  len = strlen(str);
  for(i = 0; i < len; i++)
  {
    if(str[i]>='0'&&str[i]<='9')
    {
	(*val_p)+=((str[i]-'0')<<(4*(len-i-1)));
    }
    else if(str[i]>='a'&&str[i]<='f')
    {
	(*val_p)+=((str[i]-'a'+0xa)<<(4*(len-i-1)));
    }
    else
    {
	return 1;
    }
  }
  return 0;
}