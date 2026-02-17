/* ********************************************************************
   * Project   : Command line parser
   * Author    : Simon Martin
   * Copyright : LGPL v3
   ********************************************************************

    Modifications:
    0.01 23/05/2024 Initial version.
*/

/*---------------------------------------------------------------------
  -- compatibility
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/
#include "CommandLine.h"

/*---------------------------------------------------------------------
  -- C standard includes
  ---------------------------------------------------------------------*/
#include <getopt.h>

/*---------------------------------------------------------------------
  -- C++ standard includes
  ---------------------------------------------------------------------*/
#include <algorithm>
#include <iostream>
#include <cstring>
#include <ranges>

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/
static const std::vector<std::string> empty{ "" };

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

CommandLine::CommandLine(const std::vector<CommandLineOption> &options)
{
    this->options = options;
}

void CommandLine::AddOption(const std::string_view &long_name, char short_name, bool required, HasValue has_value, Occurs occurs_type, int occurs_value, const std::string_view &help)
{
    CommandLineOption option;

    option.long_name = long_name;
    option.short_name = short_name;
    option.required = required;
    option.has_value = has_value;
    option.occurs_type = occurs_type;
    option.occurs_value = occurs_value;
    option.help = help;

    options.push_back(option);
}

void CommandLine::BuildParserParameters(std::string &short_opts, std::vector<struct option> &long_opts)
{
    // Build the options
    for (auto i = options.begin(); i != options.end(); ++i)
    {
        // Clear the flags
        i->present = 0;
        i->count = 0;

        // Add the long option
        if (!i->long_name.empty())
        {
            struct option option{};
            option.name = i->long_name.c_str();
            if (i->has_value == HasValue::Required)
            {
                option.has_arg = required_argument;
            }
            else if (i->has_value == HasValue::Optional)
            {
                option.has_arg = optional_argument;
            }
            else
            {
                option.has_arg = no_argument;
            }
            option.flag = &i->present;
            option.val = 1;

            long_opts.push_back(option);
        }

        // Add the short option
        if (i->short_name)
        {
            short_opts += i->short_name;
            if (i->has_value == HasValue::Required)
            {
                short_opts += ":";
            }
            else if (i->has_value == HasValue::Optional)
            {
                short_opts += "::";
            }
        }
    }

    // Terminate the long options array
    {
        struct option option{};
        memset(&option, 0, sizeof(option));
        long_opts.push_back(option);
    }
}

void CommandLine::ParseCommandLine(int argc, char * argv[], const std::string &short_opts, const std::vector<struct option> &long_opts, std::stringstream &error_message)
{
    char **argv_copy = new char * [argc];
    memcpy(argv_copy, argv, argc * sizeof(char *));

    // Parse the command line
    int opt = 0;
    optind = 1; // Make sure that we start at the first argument
    opterr = 0; // We will print our own error messages
    do
    {
        // Get the next option
        int long_opt_index = -1;
        opt = getopt_long(argc, argv_copy, short_opts.c_str(), long_opts.data(), &long_opt_index);

        // If we have processed all the options then exit
        if (opt == -1)
        {
            continue;
        }

        // If this is an error then alert the user.
        if ((opt == '?') || (opt == ':'))
        {
            error_message << "Error: Unknown option or missing value " << argv[optind - 1] << std::endl;
        }
        // If this is not a short option, then it must be a long one
        else if (!opt)
        {
            // Increment the option count
            options[long_opt_index].count++;

            // Store the value
            if (optarg)
            {
                options[long_opt_index].value.emplace_back(optarg);
            }
        }
        else
        {
            // Find the option
            auto option = FindOption(static_cast<char>(opt));
            if (option != options.end())
            {
                // If this is the option then increment the option count
                option->count++;

                // Store the value
                if (optarg)
                {
                    option->value.emplace_back(optarg);
                }
            }
        }
    } while (opt != -1);

    // Clean up our copy of the arguments
    delete[] argv_copy;
}

bool CommandLine::ValidateOptions(std::stringstream &error_message)
{
    // Assume that we are going to be successful
    bool rc = true;

    // Check that all the required options are present
    for (auto i = options.begin(); i != options.end(); ++i)
    {
        // If this option is not present then let's see if we need it.
        if (!i->count)
        {
            // If this option is required then we fail.
            if (i->required)
            {
                error_message << "Error: option " << i->long_name << " is required" << std::endl;
                rc = false;
            }

            // We're done
            continue;
        }

        // This option is here so let's validate the occurrences.
        switch (i->occurs_type)
        {
        case Occurs::AtLeast:
            if (i->count < i->occurs_value)
            {
                error_message << "Error: option " << i->long_name << " must occur at least " << i->occurs_value << " time(s)" << std::endl;
                rc = false;
            }
            break;
        case Occurs::AtMost:
            if (i->count > i->occurs_value)
            {
                error_message << "Error: option " << i->long_name << " must occur at most " << i->occurs_value << " time(s)" << std::endl;
                rc = false;
            }
            break;
        case Occurs::Exactly:
            if (i->count != i->occurs_value)
            {
                error_message << "Error: option " << i->long_name << " must occur exactly " << i->occurs_value << " time(s)" << std::endl;
                rc = false;
            }
            break;
        }
    }

    return rc;
}

bool CommandLine::Parse(int argc, char * argv[], std::stringstream &error_message)
{
    std::string short_opts;
    std::vector<struct option> long_opts;

    // Convert the option list to getopt_long structures.
    BuildParserParameters(short_opts, long_opts);

    // Extract the options from the command line
    ParseCommandLine(argc, argv, short_opts, long_opts, error_message);

    // Validate the options read
    return ValidateOptions(error_message);
}

void CommandLine::PrintUsage(char * argv[]) const
{
    // Get the executable name
    const char *executable = strrchr(argv[0], '/');
    if (!executable)
    {
        executable = argv[0];
    }
    else
    {
        executable++;
    }

    if (options.empty())
    {
        std::cerr << "Usage: " << executable << std::endl;
    }
    else
    {
        // Print introduction
        std::cerr << "Usage: " << executable << " <options>" << std::endl;
        std::cerr << "Where <options> is one or more of the following:" << std::endl;
        std::cerr << std::endl;

        // Print options
        for (const auto & option : options)
        {
            // If we have a short name then print it
            if (option.short_name)
            {
                std::cerr << "-" << option.short_name << " ";
            }
            // Otherwise just fill with blanks
            else
            {
                std::cerr << "   ";
            }

            // If we have a long name then print it
            if (!option.long_name.empty())
            {
                std::cerr << "--" << option.long_name << " ";
            }

            // If this option takes a value then print it
            if (option.has_value != HasValue::No)
            {
                std::cerr << "<value> ";
            }

            // Print the help
            std::cerr << std::endl << "\t" << option.help << std::endl;

            // Optional or required.
            std::cerr << "\t\tThis option is " << (option.required ? "required" : "optional") << "." << std::endl;

            // If this option takes a value then print it
            if (option.has_value != HasValue::No)
            {
                if (option.required)
                {
                    std::cerr << "\t\tThis option ";
                }
                else
                {
                    std::cerr << "\t\tIf this option occurs, then it ";
                }

                // Occurrences
                switch (option.occurs_type)
                {
                case Occurs::AtLeast:
                    std::cerr << "must occur at least " << option.occurs_value << " time(s) ";
                    break;
                case Occurs::AtMost:
                    std::cerr << "must occur at most " << option.occurs_value << " time(s) ";
                    break;
                case Occurs::Exactly:
                    std::cerr << "must occur exactly " << option.occurs_value << " time(s) ";
                    break;
                }

                // Line done
                std::cerr << std::endl;

                switch (option.has_value)
                {
                case HasValue::No:
                    std::cerr << "\t\tThis option has no value";
                    break;
                case HasValue::Required:
                    std::cerr << "\t\tThis option must have a value";
                    break;
                case HasValue::Optional:
                    std::cerr << "\t\tThis option may have a value";
                    break;
                }

                // Line done
                std::cerr << std::endl;
            }
        }
    }
}

std::vector<CommandLineOption>::const_iterator CommandLine::FindOption(const std::string &long_name) const
{
    return std::ranges::find_if(options, [&long_name](const CommandLineOption &value) -> bool { return value.long_name == long_name; });
}

std::vector<CommandLineOption>::const_iterator CommandLine::FindOption(const char long_name) const
{
    return std::ranges::find_if(options, [&long_name](const CommandLineOption &value) -> bool { return value.short_name == long_name; });
}

std::vector<CommandLineOption>::iterator CommandLine::FindOption(const char long_name)
{
    return std::ranges::find_if(options, [&long_name](const CommandLineOption &value) -> bool { return value.short_name == long_name; });
}

const std::vector<std::string> &CommandLine::GetOptionValues(const std::string &long_name) const
{
    auto i = FindOption(long_name);
    return (i != options.cend()) && (i->count > 0)? i->value : empty;
}

const std::vector<std::string> &CommandLine::GetOptionValues(const char short_name) const
{
    auto i = FindOption(short_name);
    return (i != options.cend()) && (i->count > 0)? i->value : empty;
}

bool CommandLine::IsOptionValue(const std::string &long_name) const
{
    auto i = FindOption(long_name);
    return i != options.cend() && (i->count > 0);
}

bool CommandLine::IsOptionValue(const char short_name) const
{
    auto i = FindOption(short_name);
    return i != options.cend() && (i->count > 0);
}
