/* ********************************************************************
   * Project   : Command line parser
   * Author    : Simon Martin
   ********************************************************************

    Modifications:
    0.01 23/05/2024 Initial version.
*/

#pragma once

/*---------------------------------------------------------------------
  -- compatibility
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- C standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- C++ standard includes
  ---------------------------------------------------------------------*/
#include <string>
#include <string_view>
#include <vector>
#include <sstream>

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- forward declarations
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
/**
 * Used to specify whether an option can take a value or not.
 */
enum class HasValue
{
    /**
     * Option does NOT have a value.
     */
    No,
    /**
     * Option does MUST have a value.
     */
    Required,
    /**
     * Option does MAY have a value.
     */
    Optional
};

/**
 * How to interpret the number of occurrences.
 */
enum class Occurs
{
    /**
     * Option must occur at least this many times.
     */
    AtLeast,
    /**
     * Option must occur at most this many times.
     */
    AtMost,
    /**
     * Option must occur exactly this many times.
     */
    Exactly,
};

struct CommandLineOption
{
    std::string long_name;
    char short_name;
    bool required;
    HasValue has_value;
    Occurs occurs_type;
    int occurs_value;
    std::string help;

    int present;
    int count;
    std::vector<std::string> value;
};

class CommandLine
{
protected:
    std::vector<CommandLineOption> options;

    /**
     * Convert the options to a short_opts and long_opts structures used by getopt_long
     *
     * @param short_opts Short options. A single letter or digit (-h, -4, ...)
     * @param long_opts Long option. A word (--help, --ipv4, ...)
     *
     * @see options
     * @see get_option
     */
    void BuildParserParameters(std::string &short_opts, std::vector<struct option> &long_opts);

    /**
     * Use the getopt_long options to extract the options from the command line.
     *
     * @param argc Number of parameters
     * @param argv Parameter array
     * @param short_opts Short options.
     * @param long_opts options.
     */
    void ParseCommandLine(int argc, char * argv[], const std::string &short_opts, const std::vector<struct option> &long_opts, std::stringstream &error_message);

    /**
     * Validate whether the command line options are consistent with the defined options.
     *
     * @return True if the option values are consistent.
     */
    bool ValidateOptions(std::stringstream &error_message);

public:
    CommandLine() = default;
    explicit CommandLine(const std::vector<CommandLineOption> &options);
    virtual ~CommandLine() = default;

    /**
     * Empty the options.
     */
    void Clean() { options.clear(); };

    /**
     * Add a command line option. All command line options must be added before calling Parse.
     * 
     * @param long_name  The long name of the option. This name must be prefixed by -- in the command line.
     * @param short_name The short name of the option. This name must be prefixed by - in the command line.
     * @param required   Whether this option must be specified.
     * @param has_value  Whether this option has a value.
     * @param occurs_type How to interpret the number of occurrences.
     * @param occurs_value  Number of occurrences for this option.
     * @param help       A help string that describes the option. This will be printed by the PrintUsage function.
     *
     * @see Occurs
     * @see HasValue
     */
    void AddOption(const std::string_view &long_name, char short_name, bool required, HasValue has_value, Occurs occurs_type, int occurs_value, const std::string_view &help);

    /**
     * Find a command line option by its long name.
     *
     * @param long_name The long name of the option.
     * @return          A const iterator to the option if found, or end() if not found.
     */
    [[nodiscard]] std::vector<CommandLineOption>::const_iterator FindOption(const std::string &long_name) const;

    /**
     * Find a command line option by its short name.
     *
     * @param short_name The short name of the option.
     * @return          A const iterator to the option if found, or end() if not found.
     */
    [[nodiscard]] std::vector<CommandLineOption>::const_iterator FindOption(char short_name) const;

    /**
     * Find a command line option by its short name.
     *
     * @param short_name The short name of the option.
     * @return          A modifiable iterator to the option if found, or end() if not found.
     */
    [[nodiscard]] std::vector<CommandLineOption>::iterator FindOption(char short_name);

    /**
     * Get the value of a command line option.
     * 
     * @param long_name The long name of the option.
     * @return          The value of the option.
     */
    [[nodiscard]] const std::vector<std::string> &GetOptionValues(const std::string &long_name) const;

    /**
     * Get the value of a command line option.
     * 
     * @param shortname The short name of the option.
     * @return          The value of the option.
     */
    [[nodiscard]] const std::vector<std::string> &GetOptionValues(char shortname) const;

    /**
     * Check if an option has a value.
     *
     * @param long_name The long name of the option.
     * @return          True if the option is present, false otherwise.
     */
    [[nodiscard]] bool IsOptionValue(const std::string &long_name) const;

    /**
     * Check if an option has a value.
     *
     * @param short_name The short name of the option.
     * @return           True if the option is present, false otherwise.
     */
    [[nodiscard]] bool IsOptionValue(char short_name) const;

    /**
     * Parse the command line arguments.
     * 
     * @param argc The number of command line arguments.
     * @param argv The command line arguments.
     * @return     True if the command line arguments were parsed successfully, false otherwise.
     */
    bool Parse(int argc, char * argv[], std::stringstream &error_message);

    /**
     * Print the help data from the options to explain what we are wre expecting.
     *
     * @param argv Command line options
     */
    void PrintUsage(char * argv[]) const;
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/
