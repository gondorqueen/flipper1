/*
Flipper is a replacement search engine for fanfiction.net search results
Copyright (C) 2017-2019  Marchenko Nikolai

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include "include/rec_calc/rec_calculator_mood_adjusted.h"
#include "include/Interfaces/genres.h"
#include <QStringList>

namespace core {


static auto ratioFilterMoodAdjusted = [](AuthorResult& author, QSharedPointer<RecommendationList> params)
{
    bool firstPass =  author.ratio <= params->maxUnmatchedPerMatch && author.matches > 0;
    author.usedRatio = params->maxUnmatchedPerMatch;
    author.usedMinimumMatrch= params->minimumMatch;

    if(!firstPass)
        return false;

    auto cleanRatio = author.matches != 0 ? static_cast<double>(author.fullListSize)/static_cast<double>(author.matches) : 999999;
    if(author.listDiff.touchyDifference.has_value())
    {
        auto authorcoef = author.listDiff.touchyDifference.value();
        if((cleanRatio > params->maxUnmatchedPerMatch) && authorcoef  >= 0.4)
        {
            //qDebug() << "skipping author: " << author.id << "with coef: "  << authorcoef  << " and ratio: " <<  cleanRatio;
            author.ratio = 999999;
        }
    }
    bool secondPass = author.ratio <= params->maxUnmatchedPerMatch && author.matches > 0;;
    return secondPass;
};

RecCalculatorImplWeighted::FilterListType RecCalculatorImplMoodAdjusted::GetFilterList(){
    return {matchesFilter, ratioFilterMoodAdjusted};
}

RecCalculatorImplMoodAdjusted::RecCalculatorImplMoodAdjusted(RecInputVectors input, genre_stats::GenreMoodData moodData):
    RecCalculatorImplWeighted(input), moodData(moodData)
{
    QStringList moodList;
    moodList << "Neutral" << "Funny"  << "Shocky" << "Flirty" << "Dramatic" << "Hurty" << "Bondy";

    for(auto authorKey: input.moods.keys())
    {
        double neutralDifference = 0., touchyDifference = 0.;
        auto authorData = input.moods[authorKey];
        for(int i= 0; i < moodList.size(); i++)
        {
            auto userValue =  interfaces::Genres::ReadMoodValue(moodList[i], moodData.listMoodData);
            auto authorValue =  interfaces::Genres::ReadMoodValue(moodList[i], authorData);
            if(i > 1)
                touchyDifference += std::max(userValue, authorValue) - std::min(userValue, authorValue);
            neutralDifference += std::max(userValue, authorValue) - std::min(userValue, authorValue);
        }
        if(authorKey == 77257)
        {
            qDebug() << "Logging user mood list";
            moodData.listMoodData.Log();
            qDebug() << "Logging author mood list";
            authorData.Log();
            qDebug() << "between author: " << authorKey << " and user, neutral: " << neutralDifference;
            qDebug() << "between author: " << authorKey << " and user, touchy: " << touchyDifference;
        }
        moodDiffs[authorKey].neutralDifference =  neutralDifference;
        moodDiffs[authorKey].touchyDifference =  touchyDifference;
    }
    votesBase = 100;
}

std::optional<double> RecCalculatorImplMoodAdjusted::GetNeutralDiffForLists(uint32_t author)
{
    auto it = moodDiffs.find(author);
    if(it == moodDiffs.end())
        return {};

    return it.value().neutralDifference;
}

std::optional<double> RecCalculatorImplMoodAdjusted::GetTouchyDiffForLists(uint32_t author)
{
    auto it = moodDiffs.find(author);
    if(it == moodDiffs.end())
        return {};

    return it.value().touchyDifference;
}

}
