// Copyright 2017 loblab
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include "ut181a.h"
#include "reader.h"
#include "writer.h"
#include "debug.h"

class Program
{
private:
    static const char *version_info;
    static const char *help_msg;
    static const char *short_options;
    static const struct option long_options[];
    static int quit_flag;

    UT181A::Device m_dmm;

    struct
    {
        bool list;
        bool monitor;
        int debug;
        const char* serial;
        int item_count;
        LPCSTR* items;
    } m_args;

public:
    Program();
    virtual ~Program();
    int Main(int argc, char **argv);

    static void SignalHandler(int signal);

protected:
    virtual int ParseArguments(int argc, char **argv);
    virtual int Help();
    virtual int Version();
    virtual int Init();
    virtual void Done();
    virtual int Run();
};


int Program::quit_flag = 0;
const char* Program::version_info = "ut181a ver 0.2 (12/16/2017), loblab";

const char* Program::help_msg = 
"UT181A USB communication tool\n"
"Usage: ut181a [options] [id1] [id2] ...\n"
"Options:\n"
"    -h|--help   : help message\n"
"    -v|--version: version info\n"
"    -s|--serial : specify serial string if multiple devices connected\n"
"    -m|--monitor: monitor mode (realtime measurement & transfer)\n"
"    -l|--list   : list records\n"
"    -d|--debug n: debug info level. default 0 for none, greater for more\n"
"id1, id2...:\n"
"    record index to dump (as CSV file)\n"
;

const char* Program::short_options = "hvd:s:ml";

const struct option Program::long_options[] = 
{
    {"help",     no_argument,       0, 'h'},
    {"version",  no_argument,       0, 'v'},
    {"debug",    required_argument, 0, 'd'},
    {"serial",   required_argument, 0, 's'},
    {"monitor",  required_argument, 0, 'm'},
    {"list",     required_argument, 0, 'l'},
    {0, 0, 0, 0}
};

void Program::SignalHandler(int signal)
{
    quit_flag = signal;
}

Program::Program()
{
    m_args.monitor = false;
    m_args.list = false;
    m_args.debug = 0;
    m_args.serial = NULL;
    m_args.item_count = 0;
    m_args.items = NULL;
}

Program::~Program()
{
    if (m_args.items)
    {
        if (g_debug >= 9)
            printf("Free memory\n");
        delete[] m_args.items;
        m_args.items = NULL;
    }
}

int Program::Help()
{
    printf("%s", help_msg);
    return 1;
}

int Program::Version()
{
    printf("%s\n", version_info);
    return 2;
}

int Program::ParseArguments(int argc, char **argv) 
{
    while (1)
    {
        // getopt_long stores the option index here.
        int option_index = 0;
        int c = getopt_long (argc, argv, short_options, long_options, &option_index);
        // Detect the end of the options.
        if (c == -1)
            break;

        switch (c)
        {
        case 0:
            // If this option set a flag, do nothing else now.
            if (long_options[option_index].flag != 0)
                break;
            printf ("option %s", long_options[option_index].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;

        case 'h':
            return Help();

        case 'v':
            return Version();

        case 'm':
            m_args.monitor = true;
            break;

        case 'l':
            m_args.list = true;
            break;

        case 's':
            m_args.serial = optarg;
            break;

        case 'd':
            m_args.debug = atoi(optarg);
            if (m_args.debug >= 9)
                printf("Option -d with value: %d\n", m_args.debug);
            break;

        case '?': 
            //invalid options return as '?'
            // getopt_long already printed an error message.
            Help();
            return -1;

        default:
            Help();
            return -1;
        }
    }

    // non-option ARGV items
    m_args.item_count = argc - optind;
    if (m_args.debug >= 9)
        printf("Positional argument count: %d\n", m_args.item_count);
    if (m_args.item_count > 0)
    {
        m_args.items = new LPCSTR[m_args.item_count];
        int i = 0;
        while (optind < argc)
            m_args.items[i++] = argv[optind++];
    }

    return 0;
}

int Program::Init()
{
    if (g_debug >= 9)
        fprintf(stderr, "Program::Init\n");
    // Register for SIGINT events
    signal(SIGINT, SignalHandler);

    if (!m_dmm.Open(m_args.serial))
    {
        fprintf(stderr, "Failed to open UT181A DMM. Please check device connection or settings.\n");
        if (m_args.serial)
            fprintf(stderr, "Is the serial string '%s' correct?\n", m_args.serial);
        return -1;
    }
    return 0;
}

void Program::Done()
{
    if (g_debug >= 9)
        fprintf(stderr, "Program::Done\n");
    m_dmm.Close();
}

int Program::Main(int argc, char **argv)
{
    int rc = ParseArguments(argc, argv);
    if (rc)
        return rc;
    g_debug = m_args.debug;
    rc = Init();
    if (rc)
        return rc;
    rc = Run();
    Done();
    return rc;
}

int Program::Run()
{
    if (g_debug >= 9)
        fprintf(stderr, "Program::Run\n");
    if (m_args.monitor)
    {
        fprintf(stderr, "Ctrl-C to quit the monitor\n");
        bool b = m_dmm.Monitor(quit_flag);
        return b ? 0 : -1;
    }

    if (m_args.list)
    {
        fprintf(stderr, "Ctrl-C to abort if it takes too long\n");
        m_dmm.ListRecord(quit_flag);
        return 0;
    }

    if (m_args.item_count > 0)
    {
        fprintf(stderr, "Ctrl-C to abort the long operation\n");
        for (int i = 0; i < m_args.item_count; i++)
        {
            if (quit_flag)
                break;
            int index = atoi(m_args.items[i]);
            m_dmm.ReceiveRecord(index, quit_flag);
        }
        return 0;
    }

    // If use -d option, will not print help message
    return g_debug > 0 ? 0 : Help();
}

int main (int argc, char** argv)
{
    Program prog;
    return prog.Main(argc, argv);
}
