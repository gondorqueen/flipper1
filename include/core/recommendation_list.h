#pragma once

#include "core/db_entity.h"
#include "reclist_author_result.h"

#include <QSharedPointer>
#include <QString>
#include <QDateTime>
namespace core {
class Fic;
class Author;

struct FicRecommendation
{
    QSharedPointer<core::Fic> fic;
    QSharedPointer<core::Author> author;
    bool IsValid(){
        if(!fic || !author)
            return false;
        return true;
    }
};

class RecommendationList;
typedef QSharedPointer<RecommendationList> RecPtr;

struct RecommendationListFicData
{
    int id = -1;
    QSet<int> sourceFics;
    QVector<int> fics;
    QVector<int> purges;
    QVector<int> matchCounts;
    QVector<double> noTrashScores;
    QVector<int> authorIds;
    QHash<int, int> matchReport;
    QHash<int, core::MatchBreakdown> breakdowns;
};


class RecommendationList : public DBEntity{
public:
    static RecPtr NewRecList() { return QSharedPointer<RecommendationList>(new RecommendationList);}
    void Log();
    void PassSetupParamsInto(RecommendationList& other);
    bool success = false;
    bool isAutomatic = true;
    bool useWeighting = false;
    bool useMoodAdjustment = false;
    bool hasAuxDataFilled = false;
    bool useDislikes = false;
    bool useDeadFicIgnore= false;
    bool assignLikedToSources = false;

    int id = -1;
    int ficCount =-1;
    int minimumMatch = -1;
    int alwaysPickAt = -2;
    int maxUnmatchedPerMatch = -1;
    int userFFNId = -1;
    int sigma2Distance = -1;

    double quadraticDeviation = -1;
    double ratioMedian = -1;

    QString name;
    QString tagToUse;

    QSet<int> ignoredFandoms;
    QSet<int> ignoredDeadFics;
    QSet<int> likedAuthors;
    QSet<int> minorNegativeVotes;
    QSet<int> majorNegativeVotes;
    QDateTime created;
    RecommendationListFicData ficData;

    QStringList errors;
};

}
