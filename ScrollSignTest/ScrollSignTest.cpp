// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Small example how to scroll text.
//
// This code is public domain
// (but note, that the led-matrix library this depends on is GPL v2)
// VisualGDB if header references are missing look into properties, intellisense directories
#include "led-matrix.h"
#include "graphics.h"
#include "pugixml.hpp"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bits/stdc++.h>
#include <regex>
#include <algorithm>    // std::random_shuffle
#include <unistd.h>		// Used for usleep
#include <curl/curl.h>  // Used for downloading webpage source (xml)
#include <curl/easy.h>


using namespace rgb_matrix;
using namespace std;

static int usage(const char *progname) {
	fprintf(stderr, "usage: %s [options]\n", progname);
	fprintf(stderr,
		"Reads text from rss feeds and scrolls it. ");
	fprintf(stderr, "Options:\n");
	rgb_matrix::PrintMatrixFlags(stderr);
	fprintf(stderr,
		"\t-f <font-file>    : Use given font.\n"
		"\t-b <brightness>   : Sets brightness percent. Default: 100.\n"
		"\t-C <r,g,b>        : Color. Default Random\n"
		"\t-B <r,g,b>        : Background-Color. Default 0,0,0\n");
	return 1;
}

static bool parseColor(Color *c, const char *str) {
	return sscanf(str, "%hhu,%hhu,%hhu", &c->r, &c->g, &c->b) == 3;
}

string trim(const string& str)
{
	size_t first = str.find_first_not_of(' ');
	if (string::npos == first)
	{
		return str;
	}
	size_t last = str.find_last_not_of(' ');
	return str.substr(first, (last - first + 1));
}

static size_t WriteCurlCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

// Get current date/time, format is 12hr HH:mm (am/pm %p)
const std::string currentTime() {
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	// Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
	// for more information about date/time format
	strftime(buf, sizeof(buf), " %I:%M %p", &tstruct);

	return buf;
}



vector<string> getURLs(string position)
{
	vector <string> urls;
	
	string XML_FILE_PATH = "";
	if (position == "top")
	{
		//XML_FILE_PATH = "/tmp/VisualGDB/e/Projects/ScrollSignTest/ScrollSignTest/TopFeeds.xml";
		XML_FILE_PATH = "TopFeeds.xml";
	}
	else if (position == "bottom")
	{
		//XML_FILE_PATH = "/tmp/VisualGDB/e/Projects/ScrollSignTest/ScrollSignTest/BottomFeeds.xml";
		XML_FILE_PATH = "BottomFeeds.xml";
	}
	
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(XML_FILE_PATH.c_str());
	fprintf(stderr, "Load result: %s\n", result.description());
	
	pugi::xml_node feeds = doc.document_element();	
		
	for (pugi::xml_node ch_node = feeds.child("item"); ch_node; ch_node = ch_node.next_sibling("item"))
	{			
		string url = ch_node.child("url").child_value();

		fprintf(stderr, "URL: %s\n", url.c_str());
		urls.push_back(trim(url));
	}			

	doc.reset();
	
	return urls;	
}

vector<string> getStaticText(string position)
{
	vector<string> lines;
	
	string XML_FILE_PATH = "";
	if(position == "top")
	{
		//XML_FILE_PATH = "/tmp/VisualGDB/e/Projects/ScrollSignTest/ScrollSignTest/TopLines.xml";
		XML_FILE_PATH = "TopLines.xml";
	}
	else if(position == "bottom")
	{
		//XML_FILE_PATH = "/tmp/VisualGDB/e/Projects/ScrollSignTest/ScrollSignTest/BottomLines.xml";
		XML_FILE_PATH = "BottomLines.xml";
	}
	
	pugi::xml_document doc;
	doc.load_file(XML_FILE_PATH.c_str());
	pugi::xml_node feeds = doc.child("lines");	
		
	for (pugi::xml_node ch_node = feeds.child("item"); ch_node; ch_node = ch_node.next_sibling("item"))
	{			
		string title = ch_node.child("text").child_value();
		fprintf(stderr, "Static Text: %s\n", title.c_str());
		lines.push_back(trim(title));
	}		

	doc.reset();
	
	return lines;
}


string getRegexStr()
{
	string XML_FILE_PATH = "Settings.xml";
	
	pugi::xml_document doc;
	doc.load_file(XML_FILE_PATH.c_str());
	pugi::xml_node root = doc.child("settings");	
	
	string regexStr = root.child("regex").child("string").child_value();

	doc.reset();
	
	return regexStr;
}

string getRssXml(string url)
{
	CURL *curl;
	//CURLcode res;
	std::string readBuffer;
	readBuffer = "";

	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCurlCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		/* some servers don't like requests that are made without a user-agent field, so we provide one */ 
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/60.0.3112.113 Safari/537.36");
		//res = curl_easy_perform(curl);
		curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}
	return readBuffer;
}

void displayFeeds(RGBMatrix *&canvas, string position, string fontFile)
{
	rgb_matrix::Font font;
	font.LoadFont(fontFile.c_str());
	
	Color bg_color(0, 0, 0); 
	
	vector<string> urls;	// get a 'list' of all RSS urls we are going to use.
	vector<string> titles;	// create a list to hold all of our RSS news Titles
	vector<string> textLines;
	vector<string> eventLines;
	
	// And what row the Horizontal Row text should start on	
	int y;
	int speed;
	string rowName;
	
	time_t startTime;
	time(&startTime);
	int firstPass = 1;
	double seconds;
	while(1<2)
	{
		time_t curTime;
		time(&curTime);
		seconds = difftime(curTime, startTime);
		fprintf(stderr, "Seconds: Row: %s Seconds since last update: %f\n", position.c_str(), seconds);
		
		// If its the first pass run.  If not make sure it's been at least 2 minutes since we tried to grab the news articles again
		// if it hasn't just use existing data.
		if(position == "top" && (firstPass == 1 | seconds > 120))
		{
			titles.clear();	
			rowName = "Top";
			y = 0;
			speed = 11;
			urls = getURLs("top");
			textLines = getStaticText("top");
			firstPass = 0;
		}		
		else if(position == "bottom" && (firstPass == 1 | seconds > 120))
		{
			titles.clear();	
			rowName = "Bottom";
			y = 16;
			speed  = 14;
			urls = getURLs("bottom");
			textLines = getStaticText("bottom");
			firstPass = 0;
		}
		
		string regexStr = getRegexStr();
		std::regex regexReplace(regexStr);
		
		// Add News Titles to text vector
		for (int i = 0; (unsigned)i < urls.size(); i++)
		{
			std::string url = urls[i];
			std::string returnedXML;
			returnedXML = getRssXml(url);
	 
			pugi::xml_document doc;
			doc.load_string(returnedXML.c_str());
			pugi::xml_node channel = doc.child("rss").child("channel");	
		
			for (pugi::xml_node ch_node = channel.child("item"); ch_node; ch_node = ch_node.next_sibling("item"))
			{			
				string origTitle = ch_node.child("title").child_value();
				string title = "";
				if (url == "https://www.history.com/.rss/full/this-day-in-history")
				{
					string description = ch_node.child("description").child_value();
					title = "Today in History: " + std::regex_replace(origTitle.c_str(), regexReplace, "") + " " + std::regex_replace(description.c_str(), regexReplace, "");
				}
				else
				{
					title = std::regex_replace(origTitle.c_str(), regexReplace, "");
				}
				
				
				fprintf(stderr, "Title: %s\n", title.c_str());
				titles.push_back(trim(title));
			}
		
			returnedXML.clear();
			doc.reset();
		}
		urls.clear();
		
		// Add static entries to text vector
		for (int i = 0; (unsigned)i < textLines.size(); i++)
		{
			titles.push_back(trim(textLines[i]));
		}
		textLines.clear();
		
		// Add event entries to text vector
		for (int i = 0; (unsigned)i < eventLines.size(); i++)
		{
			titles.push_back(trim(eventLines[i]));
		}
		eventLines.clear();
	
		// RANDOMIZE TITLES
		std::random_shuffle(titles.begin(), titles.end());
		
		for (int i = 0; (unsigned)i < titles.size(); i++)
		{					
			string title;
			if (position == "top")
			{
				title = titles[i];
			}
			else if (position == "bottom")
			{
				title = titles[i] + " " + currentTime().c_str();
			}					
			
			// New color for news 
			int red = 0 + rand() % 255;
			int green = 0 + rand() % 255;
			int blue = 0 + rand() % 255;
			while (red + green + blue < 50)
			{
				red = 0 + rand() % 255;
				green = 0 + rand() % 255;
				blue = 0 + rand() % 255;
			}		
			Color newColor(red, green, blue);
		
			// Turn String into Char array.  Truncate anything over 1024 characters
			char rssTitleChars[1024];
			strncpy(rssTitleChars, title.c_str(), sizeof(rssTitleChars));
			rssTitleChars[sizeof(rssTitleChars) - 1] = 0;
	
			int titleLength = title.length();
			//drawlength is number of characters * (fontwidth+)
			int drwLength = (titleLength * 9);
		
			fprintf(stderr, "Red:%s Green:%s Blue:%s\n%s: %s\n", to_string(red).c_str(), to_string(green).c_str(), to_string(blue).c_str(), rowName.c_str(), title.c_str());
			
			// if it's small enough lets do something other than scrolling it.
			// Put some randomization in there so it doesn't do it every time.  BORING!
			if (drwLength < (canvas->width() - 1) && (0 + rand() % 10 < 3))
			{
				// Blink Text in center of screen
				int textStart = round(((canvas->width() - drwLength) / 2.0));
				for (int r = 0; (unsigned)r < 6; r++)
				{		
					rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
					usleep(1000 * 1000); // 1000 * 1000 equals one second	
					rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), Color(0, 0, 0), &bg_color, rssTitleChars);
					usleep(500 * 1000); // 1000 * 1000 equals one second
				}
			}
			else if (drwLength < (canvas->width() - 1) && (0 + rand() % 10 < 8))
			{
				// center the text
				int textStart = round(((canvas->width() - drwLength) / 2.0));
				for (int r = 0; (unsigned)r < 8; r++)
				{	
					if (0 + rand() % 10 < 3)
					{
						// Erase Top down
						rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
						usleep(1000 * 1000); // 1000 * 1000 equals one second	
						if (position == "top")
						{
							for (int h = 0; (unsigned)h < 16; h++)
							{
								DrawLine(canvas, 0, h, canvas->width(), h, Color(0, 0, 0));
								usleep(100 * 1000);
							}
						}
						else if (position == "bottom")
						{
							for (int h = 16; (unsigned)h < 32; h++)
							{
								DrawLine(canvas, 0, h, canvas->width(), h, Color(0, 0, 0));
								usleep(100 * 1000);
							}
						}
					}
					else if (0 + rand() % 10 < 4)
					{
						//Erase bottom up
						rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
						usleep(1000 * 1000); // 1000 * 1000 equals one second	
						if (position == "top")
						{
							for (signed int h = 15; h >= 0; h--)
							{
								DrawLine(canvas, 0, h, canvas->width(), h, Color(0, 0, 0));
								usleep(100 * 1000);
							}
						}
						else
						{
							for (int h = 31; (unsigned)h >= 16; h--)
							{
								DrawLine(canvas, 0, h, canvas->width(), h, Color(0, 0, 0));
								usleep(100 * 1000);
							}
						}					
					}
					else if (0 + rand() % 10 < 5)
					{
						// Erase Left to Right	
						rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
						usleep(1000 * 1000); // 1000 * 1000 equals one second	
						if (position == "top")
						{
							for (int w = 0; (unsigned)w < canvas->width(); w++)
							{
								DrawLine(canvas, w, 0, w, 15, Color(0, 0, 0));
								usleep(20 * 1000);
							}
						}
						else if (position == "bottom")
						{
							for (int w = 0; (unsigned)w < canvas->width(); w++)
							{
								DrawLine(canvas, w, 16, w, 31, Color(0, 0, 0));
								usleep(20 * 1000);
							}
						}						
					}
					else if (0 + rand() % 10 < 6)
					{
						// Erase Right to Left
						rgb_matrix::DrawText(canvas, font, textStart, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
						usleep(1000 * 1000); // 1000 * 1000 equals one second	
						if (position == "top")
						{
							for (signed int w = canvas->width(); w >= 0; w--)
							{
								DrawLine(canvas, w, 0, w, 15, Color(0, 0, 0));
								usleep(20 * 1000);
							}
						}
						else if (position == "bottom")
						{
							for (signed int w = canvas->width(); w >= 0; w--)
							{
								DrawLine(canvas, w, 16, w, 31, Color(0, 0, 0));
								usleep(20 * 1000);
							}
						}
					}					
				}
			}
			// Else Just scroll the message.
			else
			{
				for (int r = 0; (unsigned)r < 2; r++)
				{		
					for (int n = (canvas->width()); n > -drwLength - 40; --n)
					{			
						//canvas->Clear();				
						rgb_matrix::DrawText(canvas, font, n, y + font.baseline(), newColor, &bg_color, rssTitleChars);	
						usleep(speed * 1000); // 1000 * 1000 equals one second
					}
				}
			}					
		}
		
	}
}

int main(int argc, char *argv[]) {
	RGBMatrix *canvas = rgb_matrix::CreateMatrixFromFlags(&argc, &argv);

	Color color(0,0,0);
	Color bg_color(0, 0, 0);
	const char *bdf_font_file = NULL;
	int brightness = 100;

	int opt;
	while ((opt = getopt(argc, argv, "f:C:B:b:")) != -1) {
		switch (opt) {
		case 'b': brightness = atoi(optarg); break;
		case 'f': bdf_font_file = strdup(optarg); break;
		case 'C':
			if (!parseColor(&color, optarg)) {
				fprintf(stderr, "Invalid color spec: %s\n", optarg);
				return usage(argv[0]);
			}
			break;
		case 'B':
			if (!parseColor(&bg_color, optarg)) {
				fprintf(stderr, "Invalid background color spec: %s\n", optarg);
				return usage(argv[0]);
			}
			break;
		default:
			return usage(argv[0]);
		}
	}

	if (bdf_font_file == NULL) {
		fprintf(stderr, "Need to specify BDF font-file with -f\n");
		return usage(argv[0]);
	}

	if (canvas == NULL)
		return 1;

	/*
	* Load font. This needs to be a filename with a bdf bitmap font.
	*/
	rgb_matrix::Font font;
	if (!font.LoadFont(bdf_font_file)) {
		fprintf(stderr, "Couldn't load font '%s'\n", bdf_font_file);
		return usage(argv[0]);
	}

	if (brightness < 1 || brightness > 100) {
		fprintf(stderr, "Brightness is outside usable range.\n");
		return 1;
	}

	canvas->SetBrightness(brightness);

	// Set the Color Depth of the Sign 1..11
	// Less than 6 and some colors will not get shown at all
		canvas->SetPWMBits(8);
	
	std::thread topThread(displayFeeds, ref(canvas), "top", bdf_font_file);
	std::thread btmThread(displayFeeds, ref(canvas), "bottom", bdf_font_file);
	topThread.join();
	btmThread.join();
		
	usleep(1000 * 1000); // 100 * 1000 equals one second
	
	// Finished. Shut down the RGB matrix.
	canvas->Clear();
	delete canvas;

	return 0;
}

