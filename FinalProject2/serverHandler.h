#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
using namespace std;

#pragma once
namespace serverHandler
{
    enum duration { days, weeks, months, years };
    httplib::Result getCurrencyInfo(int year, int month, int day, string currency);
    map<string, double> getInfo(int year, int month, int day, const string& mainCurrency, const string& secCurrency, duration d, int multiplication);

    // Helper function prototypes
    void adjustDate(int& year, int& month, int& day, int daysToSubtract);
    void adjustMonth(int& year, int& month, int day, int monthsToSubtract);
    void adjustYear(int& year, int& monthsToSubtract);
    double getValueFromJson(const string& json, const string& key);
    string formatDate(int year, int month, int day);
};
