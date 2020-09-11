#pragma once
#include "discord/command_creators.h"

template <typename T>
struct TypeStringHolder{
};
template <>
struct TypeStringHolder<discord::RecsCreationCommand>{
        static constexpr std::string_view prefixlessPattern = "recs";
        static constexpr std::string_view pattern = "recs\\s{1,}(\\d{4,10})";
        static constexpr std::string_view help = "Basic commands:\n`!recs FFN_ID` to create recommendations";
};
template <>
struct TypeStringHolder<discord::NextPageCommand>{
        static constexpr std::string_view prefixlessPattern = "next";
        static constexpr std::string_view pattern = "next";
        static constexpr std::string_view help = "`!next` to navigate to the next page of the recommendation results";
};
template <>
struct TypeStringHolder<discord::PreviousPageCommand>{
        static constexpr std::string_view prefixlessPattern = "prev";
        static constexpr std::string_view pattern = "prev";
        static constexpr std::string_view help = "`!prev` to navigate to the previous page of the recommendation results";
};
template <>
struct TypeStringHolder<discord::PageChangeCommand>{
        static constexpr std::string_view prefixlessPattern = "page";
        static constexpr std::string_view pattern = "page\\s{1,}(\\d{1,10})";
        static constexpr std::string_view help = "`!page X` to navigate to a differnt page in recommendation results";
};
template <>
struct TypeStringHolder<discord::SetFandomCommand>{
        static constexpr std::string_view prefixlessPattern = "fandom";
        static constexpr std::string_view pattern = "fandom(\\s{1,}>pure){0,1}(\\s{1,}>reset){0,1}(\\s{1,}.+){0,1}";
        static constexpr std::string_view help = "\nFandom filter commands:\n`!fandom X` for single fandom searches"
                                                 "\n`!fandom >pure X` if you want to exclude crossovers "
                                                 "\n`!fandom` a second time with a diffent fandom if you want to search for exact crossover"
                                                 "\n`!fandom >reset` to reset fandom filter";
};
template <>
struct TypeStringHolder<discord::IgnoreFandomCommand>{
        static constexpr std::string_view prefixlessPattern = "xfandom";
        static constexpr std::string_view pattern = "xfandom(\\s{1,}>full){0,1}(\\s{1,}>reset){0,1}(\\s{1,}.+){0,1}";
        static constexpr std::string_view help = "\nFandom ignore commands:\n`!xfandom X` to permanently ignore fics just from this fandom or remove an ignore"
                                                 "\n`!xfandom >full X` to also ignore crossovers from this fandom,"
                                                 "\n`!xfandom >reset` to reset fandom ignore list";
};
template <>
struct TypeStringHolder<discord::IgnoreFicCommand>{
        static constexpr std::string_view prefixlessPattern = "xfic";
        static constexpr std::string_view pattern = "xfic((\\s{1,}\\d{1,2}){1,10})|(\\s{1,}>all)";
        static constexpr std::string_view help = "\nFanfic commands:\n`!xfic X` will ignore a fic (you need input position in the last output), X Y Z to ignore multiple"
                                                 "\n`!xfic >all` will ignore the whole page";
                                                 //"\n`!xfic >reset` resets the fic ignores";
};
template <>
struct TypeStringHolder<discord::DisplayHelpCommand>{
        static constexpr std::string_view prefixlessPattern = "help";
        static constexpr std::string_view pattern = "help";
        static constexpr std::string_view help = "`!help` display this text";
};
template <>
struct TypeStringHolder<discord::RngCommand>{
        static constexpr std::string_view prefixlessPattern = "roll";
        static constexpr std::string_view pattern = "roll\\s(best|good|all)";
        static constexpr std::string_view help = "`\n!roll best/good/all` will display a set of 3 random fics from within a selected range in the recommendations.";
};
template <>
struct TypeStringHolder<discord::ChangeServerPrefixCommand>{
        static constexpr std::string_view prefixlessPattern = "prefix";
        static constexpr std::string_view pattern = "prefix(\\s.+)";
        static constexpr std::string_view help = "\nBot management commands:\n`!prefix new prefix` changes the comamnd prefix for this server(admin only)";
};


