#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

class CommandLineParser
{
  public:
    class Option  
    {
      public:
        using integerVariant = std::variant<int *, uint32_t *>;
        using SupportedValues = std::variant<std::string *, std::vector<std::string> *, bool *, integerVariant>;

        explicit Option(std::vector<std::string> flags, SupportedValues value, std::string help) noexcept;

        std::vector<std::string> const &flags() const noexcept;

        SupportedValues value() const noexcept;

        std::string help() const noexcept;

        Option(const Option &) = delete;
        Option &operator=(const Option &) = delete;
        Option(Option &&other) noexcept;

        ~Option() = default;

      private:
        std::vector<std::string> _flags;
        SupportedValues _value;
        std::string _help;
    };

    explicit CommandLineParser(std::string_view description);

    void addOption(Option option);

    void parse(const std::vector<std::string_view> &argumentList) const;

    void printHelp(std::ostream &os = std::cout) const;

  private:
    bool handleArgument(const Option &argument, const std::string &value, std::string_view flag, bool &valueIsSeparated) const;

    const std::string_view _description;
    std::vector<Option> _arguments;
};
