/*
Flipper is a recommendation and search engine for fanfiction.net
Copyright (C) 2017-2018  Marchenko Nikolai

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
#pragma once
#include "Interfaces/base.h"
#include "Interfaces/db_interface.h"
#include "discord/limits.h"
#include "discord/discord_user.h"
#include "core/section.h"

#include "GlobalHeaders/SingletonHolder.h"

#include <QScopedPointer>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QReadWriteLock>

#include <mutex>



namespace interfaces {
class IDBWrapper;
class Fandoms;

class Users {
public:
    virtual ~Users() = default;
    QSharedPointer<discord::User> GetUser(QString);
    void WriteUser(QSharedPointer<discord::User>);
    void WriteUserFFNId(QString user_id, int ffn_id);
    void WriteUserFavouritesSize(QString, int);
    int WriteUserList(QString user_id,
                      QString list_name, discord::EListType list_type,
                      int min_match, int match_ratio, int always_at);
    bool WriteForcedListParams(QString user_id, int forceMinMatches, int forcedRatio);
    bool WriteForceLikedAuthors(QString user_id, bool);
    bool WriteFreshSortingParams(QString user_id, bool, bool);
    bool WriteLargeListReparseToken(QString user_id, discord::LargeListToken);
    bool SetHideDeadFilter(QString user_id, bool);
    bool SetCompleteFilter(QString user_id, bool);

    bool DeleteUserList(QString user_id,
                   QString list_name);
    void IgnoreFandom(QString userId, int fandomId, bool ignoreCrosses);
    void UnignoreFandom(QString userId, int fandomId);
    discord::FandomFilter GetIgnoreList(QString userId);
    void TagFanfic(QString userId, QString tag, int ficId);
    void UnTagFanfic(QString userId, QString tag, int ficId); // empty tag removes all
    void BanUser(QString userId);
    void UnbanUser(QString userId);
    void UpdateCurrentPage(QString userId, int page);
    void UnfilterFandom(QString userId, int fandomId);
    void ResetFandomFilter(QString userId);
    void ResetFandomIgnores(QString userId);
    void ResetFicIgnores(QString userId);
    void FilterFandom(QString userId, int fandomId, bool allowCrossovers);
    void CompletelyRemoveUser(QString userId);
    void SetWordcountFilter(QString userId, discord::WordcountFilter);
    void SetDeadFicDaysRange(QString userId, int);
};

}
BIND_TO_SELF_SINGLE(interfaces::Users);

