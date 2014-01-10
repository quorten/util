//sVOLExtract.cpp - main hacking Driver's Education
//Basic reassembling may latter be included
//None of this should be made for reusablility because it is considered (to me) a legacy file format
//There are many other ways to access archived data, even TAR

#define WIN32_LEAN_AND_MEAN
#include <windows.h> //Use windows file routines
#include <memory.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

//This function will always split files unless -i option is specified at command line
bool ExtractSoundVOL(bool bBundle = false) //bool bBundle: make file group into legacy archive if true
{
	//Source file-related variables
	char* volFileChunk; //Entire file in memory
	long volFileSize;
	long dataIndex = 0;
	HANDLE volFile;
	DWORD bytesRead; //Used by ReadFile()

	//Destination file-related variables
	vector<long> soundData; //Pointers to the sound data (data locations in volFileChunk)
	vector<long> soundDataSize;
	vector<string> soundNames; //The captions or assigned untitled names. Used as the file name.
	int numSounds = 0;
	bool bWaitingCaption = false;
	string waitingCaption;
	int numUntitledCapts = 0; //number of untitled captions
	HANDLE destFile;
	DWORD bytesWritten; //Used by WriteFile()

	volFile = CreateFile("sound.vol", GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
	if (volFile == INVALID_HANDLE_VALUE)
	{
		MessageBox(NULL, "Could not open sound VOL file!", "File Error", MB_OK | MB_ICONERROR);
		//delete []volFileChunk;
		return false;
	}

	//Load the whole file into memory (about 48MB, memory will be cross-referenced)(source file)
	volFileSize = GetFileSize(volFile, NULL);
	volFileChunk = new char[volFileSize];
	ReadFile(volFile, volFileChunk, volFileSize, &bytesRead, NULL);
	CloseHandle(volFile); //We are done with the disk contents in this kind of operation


	//Now we format all the data into separate files (preprocess destination files)

	//Verify that the file has the size set up right by reading first DWORD
	long volIndirectSize;
	memcpy(&volIndirectSize, &volFileChunk[dataIndex], 4);
	/*Skip this incompatible check (for now)
	if ((volIndirectSize + 4) != volFileSize) //Something is really wrong
	{
		char errorMsg[256];
		sprintf(errorMsg, "Internal file size for VOL file\nis not equal to real file size!\nDebug info: #1:%i #2:%i", volIndirectSize, volFileSize);
		MessageBox(NULL, errorMsg, "Something's wrong here", MB_OK | MB_ICONERROR);
		delete []volFileChunk;
		return false;
	}*/
	dataIndex += 4;
	while (dataIndex < volFileSize) //index through all the data
	{
		//Check if there is soundwave data first, some sound waves don't have captions (cars)
		if (memcmp(&volFileChunk[dataIndex], "RIFF", 4) == 0)
		{
			cout << "sound found.\n";
			//Make a pointer to this location and get the data size
			soundData.push_back(dataIndex);
			dataIndex += 4;
			long sizeNum = 0;
			memcpy(&sizeNum, &volFileChunk[dataIndex], 4);
			sizeNum += 4;
			cout << "Size identity: " << sizeNum << endl;
			soundDataSize.push_back(sizeNum);
			//Check if there is a waiting caption, or else make our own untitled caption
			if (bWaitingCaption)
			{
				soundNames.push_back(waitingCaption);
				bWaitingCaption = false;
			}
			else
			{
				numUntitledCapts++;
				cout << numUntitledCapts << " untitled captions have been found so far\n";
				char temp[100];
				sprintf(temp, "Untitled%i", numUntitledCapts);
				string untitledCapt = temp;
				cout << untitledCapt << endl;
				soundNames.push_back(untitledCapt);
			}
			//Skip past the rest of the data
			dataIndex += sizeNum;

			numSounds++;
		}
		else //Get a caption, stop at RIFF identifier
		{
			cout << "caption found.\n";
			int stringLength = 0;
			char* tempString;
			while (true)
			{
				if (memcmp(&volFileChunk[dataIndex+stringLength], "RIFF", 4) == 0)
				{
					//stringLength++; //Hold the 0x0A byte too
					tempString = new char[stringLength+1]; //Null character is the +1
					ZeroMemory(tempString, stringLength+1);
					memcpy(tempString, &volFileChunk[dataIndex], stringLength);
					waitingCaption = tempString;
					delete []tempString;
					int badchar = waitingCaption.find("\x0D"); //Get rid of newline and other non-file characters
					if (badchar != string::npos)
						waitingCaption.erase(badchar, 2);
					badchar = waitingCaption.find("\x0D"); //Try again to make sure all are removed
					if (badchar != string::npos)
						waitingCaption.erase(badchar, 2);
					badchar = waitingCaption.find("\x0D"); //One more time, just to be safe
					if (badchar != string::npos)
						waitingCaption.erase(badchar, 2);
					badchar = waitingCaption.find(":");
					if (badchar != string::npos)
						waitingCaption.erase(badchar, 1);
					badchar = waitingCaption.find("?");
					if (badchar != string::npos)
						waitingCaption.erase(badchar, 1);
					if (waitingCaption.size() > 100)
						waitingCaption.erase(100); //Truncate string (this is not the maximum)
					cout << "caption: " << waitingCaption << endl; //Diagnostic
					break;
				}
				else
					stringLength++;
			}
			bWaitingCaption = true;
			dataIndex += stringLength; //Skip after the string
		}
		if (dataIndex == 0x0303C5DA) //Fatal, more info (we don't care about it, we will need a new collecting way to make that data valued, but this is just a simple way)
			break; //We're done... export the collected data
	}

	//Now we used the collected referenced data to make the output files
	for (int i = 0; i < numSounds; i++)
	{
		string filename = soundNames[i] + ".wav"; //Append extension
		for (int j = 2; j < 102; j++) //Try at most 100 different filename combinations
		{
			destFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (destFile == INVALID_HANDLE_VALUE)
			{
				char temp[20];
				sprintf(temp, " %i.wav", j);
				filename = soundNames[i] + temp;
			}
			else
				break;
		}
		//Give up
		if (destFile == INVALID_HANDLE_VALUE)
		{
			char temp[200];
			sprintf(temp, "The current destination file has a faulty file name!\nName: %s", filename.c_str());
			MessageBox(NULL, temp, "File Error", MB_OK | MB_ICONERROR);
			//continue;
		}

		WriteFile(destFile, &volFileChunk[soundData[i]], 4 + soundDataSize[i], &bytesWritten, NULL);
		CloseHandle(destFile);
	}

	//Summerize diagnostic information
	cout << "\nSummery of sound.vol:\nSize in bytes: " << volFileSize << "\nTotal number of sounds: " << numSounds << "\nTotal number of untitled sounds: " << numUntitledCapts << "\nTotal number of captions: " << soundNames.size() << endl;


	//Free up the dynamically allocated memory
	delete []volFileChunk;
	return true;
}

main(int argc, char* argv[])
{
	ExtractSoundVOL();
}
