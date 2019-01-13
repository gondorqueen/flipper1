/*
Flipper is a replacement search engine for fanfiction.net search results
Copyright (C) 2017  Marchenko Nikolai

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
#include "include/servitorwindow.h"
#include "include/Interfaces/genres.h"
#include "include/Interfaces/fanfics.h"
#include "include/Interfaces/fandoms.h"
#include "include/Interfaces/interface_sqlite.h"
#include "include/Interfaces/authors.h"
#include "include/Interfaces/recommendation_lists.h"
#include "include/Interfaces/ffn/ffn_fanfics.h"
#include "include/Interfaces/ffn/ffn_authors.h"
#include "include/url_utils.h"
#include "include/favholder.h"


#include "include/tasks/slash_task_processor.h"
#include <QTextCodec>
#include <QSettings>
#include <QSqlRecord>
#include <QFuture>
#include <QVBoxLayout>
#include <QMutex>
#include <QtConcurrent>

#include <QFutureWatcher>
#include "ui_servitorwindow.h"
#include "include/parsers/ffn/ficparser.h"
#include "include/parsers/ffn/favparser.h"
#include "include/timeutils.h"
#include "include/page_utils.h"
#include "pagegetter.h"
#include "tasks/recommendations_reload_precessor.h"
#include <type_traits>
#include <algorithm>
//#include <QtCharts>
#include <QChartView>
#include <QChart>
#include <QBarSeries>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QValueAxis>


ServitorWindow::ServitorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::servitorWindow)
{
    ui->setupUi(this);
    qRegisterMetaType<WebPage>("WebPage");
    qRegisterMetaType<PageResult>("PageResult");
    qRegisterMetaType<ECacheMode>("ECacheMode");
    //    qRegisterMetaType<FandomParseTask>("FandomParseTask");
    //    qRegisterMetaType<FandomParseTaskResult>("FandomParseTaskResult");
    ReadSettings();

    An<interfaces::GenreIndex> genreIndex;
    ui->cbGenres->blockSignals(true);

    QStringList genreList;
    genreList << "not found" << "Romance" << "Drama" << "Tragedy" << "Angst"
              << "Humor" << "Parody" << "Hurt/Comfort" << "Family" << "Friendship"
              << "Horror" << "Adventure" << "Mystery" << "Supernatural" <<  "Suspense" << "Sci-Fi"
              << "Fantasy" << "Spiritual" << "Western" << "Crime";


    for(auto i = 0; i < genreList.size(); i++)
        ui->cbGenres->addItem(genreList[i]);

    ui->cbGenres->blockSignals(false);

    ui->cbMoodSelector->blockSignals(true);

    QStringList moodList;
    moodList << "Neutral" << "Flirty" << "Funny" << "Dramatic" << "Hurty" << "Bondy" << "Shocky";


    for(auto i = 0; i < moodList.size(); i++)
        ui->cbMoodSelector->addItem(moodList[i]);

    ui->cbMoodSelector->blockSignals(false);

    CreateChartView();
}

ServitorWindow::~ServitorWindow()
{
    WriteSettings();
    delete ui;
}

void ServitorWindow::ReadSettings()
{
    QSettings settings("servitor.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    ui->leFicUrl->setText(settings.value("Settings/ficUrl", "").toString());
    ui->leReprocessFics->setText(settings.value("Settings/reprocessIds", "").toString());
}

void ServitorWindow::WriteSettings()
{
    QSettings settings("servitor.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    if(!ui->leFicUrl->text().trimmed().isEmpty())
        settings.setValue("Settings/ficUrl", ui->leFicUrl->text());
    if(!ui->leReprocessFics->text().trimmed().isEmpty())
        settings.setValue("Settings/reprocessIds", ui->leReprocessFics->text());
    settings.sync();
}

void ServitorWindow::UpdateInterval(int, int)
{

}

void ServitorWindow::CreateChartView()
{
    //using namespace QtCharts;
    QStringList categoryNames;
    categoryNames << "0" << "0.05" << "0.1" <<
                     "0.15" << "0.2" <<
                     "0.25" << "0.3" <<
                     "0.35" << "0.4" <<
                     "0.45" << "0.5" <<
                     "0.55" << "0.6" <<
                     "0.65" << "0.7" <<
                     "0.75" << "0.8" <<
                     "0.85" << "0.9" <<
                     "0.95" << "1" << "1+";
    // genres
    genreChartView.reset(new QtCharts::QChartView());
    genreChart.reset(new QtCharts::QChart());
    genreChartView->setRenderHint(QPainter::Antialiasing);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(genreChartView.data());
    ui->wdgCharts->setLayout(layout);

    QtCharts::QBarCategoryAxis *axisX = new QtCharts::QBarCategoryAxis();
    axisX->append(categoryNames);
    genreChart->addAxis(axisX, Qt::AlignBottom);
    genreAxisY.reset(new QtCharts::QValueAxis());
    genreAxisY->setRange(0,20000);
    genreChart->addAxis(genreAxisY.data(), Qt::AlignLeft);
    genreChart->setTitle("Genre distributions");
    genreChart->setAnimationOptions(QtCharts::QChart::SeriesAnimations);
    // moods

    moodChartView.reset(new QtCharts::QChartView());
    moodChart.reset(new QtCharts::QChart());
    moodChartView->setRenderHint(QPainter::Antialiasing);
    QVBoxLayout *moodLayout = new QVBoxLayout;
    moodLayout->addWidget(moodChartView.data());
    ui->wdgMoodChart->setLayout(moodLayout);

    QtCharts::QBarCategoryAxis *moodAxisX = new QtCharts::QBarCategoryAxis();
    moodAxisX->append(categoryNames);
    moodChart->addAxis(moodAxisX, Qt::AlignBottom);
    moodAxisY.reset(new QtCharts::QValueAxis());
    moodAxisY->setRange(0,20000);
    moodAxisY->setTickCount(10);
    moodChart->addAxis(moodAxisY.data(), Qt::AlignLeft);
    moodChart->setTitle("Genre distributions");
    moodChart->setAnimationOptions(QtCharts::QChart::SeriesAnimations);
}

void ServitorWindow::InitGenreChartView(QString name)
{

    // first need to create the series
    An<interfaces::GenreIndex> genreIndex;
    auto genre = genreIndex->GenreByName(name);
    if(!genre.isValid)
    {
        QLOG_INFO() << "invalid genre!";
        return;
    }
    genreChart->removeAllSeries();

    auto index = genre.indexInDatabase;
    QList<double> categories;
    categories << 0 << 0.05 << 0.1 <<
                  0.15 << 0.2 <<
                  0.25 << 0.3 <<
                  0.35 << 0.4 <<
                  0.45 << 0.5 <<
                  0.55 << 0.6 <<
                  0.65 << 0.7 <<
                  0.75 << 0.8 <<
                  0.85 << 0.9 <<
                  0.95 << 1.001 << 100;



    QVector<int> countsOriginal;
    QVector<int> countsAdjusted;
    QtCharts::QBarSet *setOriginals = new QtCharts::QBarSet("Originals");
    QtCharts::QBarSet *setAdjusted = new QtCharts::QBarSet("Adjusted");
    int maxValue = 0;
    QLOG_INFO() << "Size of original author data: " << authorGenreDataOriginal.size();
    QLOG_INFO() << "Size of adjusted author data: " << iteratorProcessor.resultingGenreAuthorData.size();
    for(int i = 0; i < categories.size() - 1; i++)
    {
        auto value = std::count_if(std::begin(authorGenreDataOriginal), std::end(authorGenreDataOriginal), [categories,index, start = i, end = i+1](const std::array<double, 22>& data){
            return data[index] >= categories[start] && data[index] < categories[end];
        });
        countsOriginal.push_back(value);
        *setOriginals << value;
        if(value > maxValue)
            maxValue = value;
        value = std::count_if(std::begin(iteratorProcessor.resultingGenreAuthorData), std::end(iteratorProcessor.resultingGenreAuthorData), [categories,index, start = i, end = i+1](const std::array<double, 22>& data){
            return data[index] >= categories[start] && data[index] < categories[end];
        });
        countsAdjusted.push_back(value);
        *setAdjusted << value;
        if(value > maxValue)
            maxValue = value;
    }


    QLOG_INFO()  << "Original distribution: "<< countsOriginal;
    QLOG_INFO()  << "Adjusted distribution: "<< countsAdjusted;


    QtCharts::QBarSeries *series = new QtCharts::QBarSeries();
    series->append(setOriginals);
    series->append(setAdjusted);
    genreChart->addSeries(series);
    genreChart->setGeometry(genreChartView->rect());
    genreChart->removeAxis(genreAxisY.data());
    genreAxisY.reset(new QtCharts::QValueAxis);
    genreAxisY->setRange(0, maxValue*1.05);
    genreAxisY->setTickCount(10);


    //    axisY->setTickCount(5);
    genreChart->setTitle(name + " distribution.");
    genreChartView->setChart(genreChart.data());
    genreChart->setAxisY(genreAxisY.data(), series);

    //chartView->chart()->setAxisY(axisY, series);

}

void ServitorWindow::InitMoodChartView(QString name)
{
    An<interfaces::GenreIndex> genreIndex;
    moodChart->removeAllSeries();
    auto moodValue = [](genre_stats::ListMoodData& data, QString name) -> float{
        if(name == "Neutral")
            return data.strengthNeutral;
        if(name == "Flirty")
            return data.strengthFlirty;
        if(name == "Funny")
            return data.strengthFunny;
        if(name == "Dramatic")
            return data.strengthDramatic;
        if(name == "Bondy")
            return data.strengthBondy;
        if(name == "Shocky")
            return data.strengthShocky;
        if(name == "Hurty")
            return data.strengthHurty;
        return 0;
    };
    //moodList << "Neutral" << "Flirty" << "Humorous" << "Dramatic" << "Hurty" << "Bondy" << "Shocky";
    QList<float> categories;
    //categories << -1 << 10.1f;
    categories << -1 << 0.05f << 0.1f <<
                  0.15f << 0.2f <<
                  0.25f << 0.3f <<
                  0.35f << 0.4f <<
                  0.45f << 0.5f <<
                  0.55f << 0.6f <<
                  0.65f << 0.7f <<
                  0.75f << 0.8f <<
                  0.85f << 0.9f <<
                  0.95f << 1.001f << 100.f;
    QVector<int> countsOriginal;
    QVector<int> countsAdjusted;
    QtCharts::QBarSet *setOriginals = new QtCharts::QBarSet("Originals");
    QtCharts::QBarSet *setAdjusted = new QtCharts::QBarSet("Adjusted");
    int maxValue = 0;
    QLOG_INFO()  << "Original distribution size: "<< originalMoodData.size();
    QLOG_INFO()  << "Adjusted distribution size: "<< adjustedMoodData.size();

    int sumOriginal = 0;
    int sumAdjusted = 0;
    for(int i = 0; i < categories.size() - 1; i++)
    {
        auto value = std::count_if(std::begin(originalMoodData), std::end(originalMoodData),
                                   [moodValue, categories,name, start = i, end = i+1](genre_stats::ListMoodData& data){
            return moodValue(data, name) >= categories[start] && moodValue(data, name) < categories[end];
        });
        countsOriginal.push_back(value);
        sumOriginal+=value;
        *setOriginals << value;
        if(value > maxValue)
            maxValue = value;
        value = std::count_if(std::begin(adjustedMoodData), std::end(adjustedMoodData),
                              [moodValue, categories,name, start = i, end = i+1](genre_stats::ListMoodData& data){
            return moodValue(data, name) >= categories[start] && moodValue(data, name) < categories[end];
        });
        countsAdjusted.push_back(value);
        sumAdjusted+=value;
        *setAdjusted << value;
        if(value > maxValue)
            maxValue = value;
    }
    QLOG_INFO()  << "Original sum: "<< sumOriginal;
    QLOG_INFO()  << "Adjusted sum: "<< sumAdjusted;

    QLOG_INFO()  << "Original distribution: "<< countsOriginal;
    QLOG_INFO()  << "Adjusted distribution: "<< countsAdjusted;


    QtCharts::QBarSeries *series = new QtCharts::QBarSeries();
    series->append(setOriginals);
    series->append(setAdjusted);
    moodChart->addSeries(series);
    moodChart->setGeometry(moodChartView->rect());
    moodChart->removeAxis(moodAxisY.data());
    moodAxisY.reset(new QtCharts::QValueAxis);
    moodAxisY->setRange(0, maxValue*1.2);
    moodAxisY->setTickCount(10);


    //    axisY->setTickCount(5);
    moodChart->setTitle(name + " distribution.");
    moodChartView->setChart(moodChart.data());
    moodChart->setAxisY(moodAxisY.data(), series);
}

static QHash<QString, int> CreateGenreRedirects(){
    QHash<QString, int> result;
    result["General"] = 0;
    result["Humor"] = 1;
    result["Poetry"] = 2;
    result["Adventure"] = 3;
    result["Mystery"] = 4;
    result["Horror"] = 5;
    result["Parody"] = 6;
    result["Angst"] = 7;
    result["Supernatural"] = 8;
    result["Suspense"] = 9;
    result["Romance"] = 10;
    result["not found"] = 11;
    result["Sci-Fi"] = 13;
    result["Fantasy"] = 14;
    result["Spiritual"] = 15;
    result["Tragedy"] = 16;
    result["Western"] = 17;
    result["Crime"] = 18;
    result["Family"] = 19;
    result["Hurt/Comfort"] = 20;
    result["Friendship"] = 21;
    result["Drama"] = 22;
    return result;
}



QVector<genre_stats::FicGenreData> ServitorWindow::CreateGenreDataForFics(GenreDetectionSources input,
                                                                          CutoffControls cutoff,
                                                                          bool userIterationForGenreProcessing, bool displayLog){
    QVector<genre_stats::FicGenreData> ficGenreDataList;
    ficGenreDataList.reserve(input.ficsToUse.keys().size());
    interfaces::GenreConverter genreConverter;
    int counter = 0;


    for(auto fic : input.ficsToUse.keys())
    {
        if(!ui->leFicIdForGenre->text().isEmpty() && fic != ui->leFicIdForGenre->text().toInt())
            continue;

        if(counter%10000 == 0)
            qDebug() << "processing fic: " << counter;

        //gettting amount of funny lists
        int64_t funny = std::count_if(std::begin(input.ficsToUse[fic]), std::end(input.ficsToUse[fic]), [&](int listId){
            return input.moodAuthorLists[listId].strengthFunny >= cutoff.funny;
        });
        int64_t flirty = std::count_if(input.ficsToUse[fic].begin(), input.ficsToUse[fic].end(), [&](int listId){
            return input.moodAuthorLists[listId].strengthFlirty >= cutoff.flirty;
        });
        auto listSet = input.ficsToUse[fic];
        int64_t neutralAdventure = 0;
        for(auto listId : listSet)
            if(input.genreAuthorLists[listId][3] >= cutoff.adventure)
                neutralAdventure++;

        //qDebug() << "Adventure list count: " << neutralAdventure;

        int64_t hurty = std::count_if(input.ficsToUse[fic].begin(), input.ficsToUse[fic].end(), [&](int listId){
            return input.moodAuthorLists[listId].strengthHurty >= cutoff.hurty;
        });
        int64_t bondy = std::count_if(input.ficsToUse[fic].begin(), input.ficsToUse[fic].end(), [&](int listId){
            return input.moodAuthorLists[listId].strengthBondy >= cutoff.bonds;
        });

        int64_t neutral = std::count_if(input.ficsToUse[fic].begin(), input.ficsToUse[fic].end(), [&](int listId){
            return input.moodAuthorLists[listId].strengthNeutral>= cutoff.adventure;
        });
        int64_t dramatic = std::count_if(input.ficsToUse[fic].begin(), input.ficsToUse[fic].end(), [&](int listId){
            return input.moodAuthorLists[listId].strengthDramatic >= cutoff.drama;
        });

        int64_t total = input.ficsToUse[fic].size();

        genre_stats::FicGenreData genreData;
        genreData.ficId = fic;
        genreData.originalGenres =  genreConverter.GetFFNGenreList(input.originalFicGenres[fic]);
        genreData.totalLists = static_cast<int>(total);
        genreData.strengthHumor = static_cast<float>(funny)/static_cast<float>(total);
        genreData.strengthRomance = static_cast<float>(flirty)/static_cast<float>(total);
        genreData.strengthDrama = static_cast<float>(dramatic)/static_cast<float>(total);
        genreData.strengthBonds = static_cast<float>(bondy)/static_cast<float>(total);
        genreData.strengthHurtComfort = static_cast<float>(hurty)/static_cast<float>(total);
        genreData.strengthNeutralComposite = static_cast<float>(neutral)/static_cast<float>(total);
        genreData.strengthNeutralAdventure = static_cast<float>(neutralAdventure)/static_cast<float>(total);
        if(displayLog)
            genreData.Log();
        if(!userIterationForGenreProcessing)
            genreConverter.ProcessGenreResult(genreData);
        else
            genreConverter.ProcessGenreResultIteration2(genreData);
        ficGenreDataList.push_back(genreData);
        counter++;
    }


    for(auto fic : ficGenreDataList)
    {
        QStringList keptList;
        for(auto genre: fic.processedGenres)
        {
            if(genre.relevance < 0.1f)
                keptList += genre.genres;
        }

        for(auto genre : fic.processedGenres)
        {
            QString writtenGenre = genre.genres.join(",");
            if(genre.relevance > fic.maxGenrePercent && !writtenGenre.isEmpty())
                fic.maxGenrePercent = genre.relevance;
        }
        fic.keptToken = keptList.join(",");
    }

    return ficGenreDataList;
}


void ServitorWindow::DetectGenres(int minAuthorRecs, int minFoundLists)
{
    interfaces::GenreConverter converter;



    QVector<int> ficIds;
    auto db = QSqlDatabase::database();
    auto genres  = QSharedPointer<interfaces::Genres> (new interfaces::Genres());
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    auto authors= QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    genres->db = db;
    fanfics->db = db;
    authors->db = db;

    An<core::RecCalculator> holder;
    holder->LoadFavourites(authors);
    qDebug() << "Finished list load";

    QHash<int, QSet<int>> ficsToUse;
    auto& faves  = holder->holder.faves;

    for(int key : faves.keys())
    {
        auto& set = faves[key];
        //qDebug() <<  key << " set size is: " << set.size();
        if(set.cardinality() < minAuthorRecs)
            continue;
        //qDebug() << "processing";
        for(auto fic : set)
        {
            //            if(fic != 38212)
            //                continue;
            ficsToUse[fic].insert(key);
        }
    }
    qDebug() << "Finished author processing, resulting set is of size:" << ficsToUse.size();



    QList<int> result;
    for(auto key : ficsToUse.keys())
    {
        if(ficsToUse[key].size() >= minFoundLists)
            result.push_back(key);

    }

    QHash<int, QSet<int>> filteredFicsToUse;
    for(auto key : ficsToUse.keys())
    {
        if(ficsToUse[key].size() >= minFoundLists)
            filteredFicsToUse[key] = ficsToUse[key];

    }

    qDebug() << "Finished counts";

    //    for(auto fic: result)
    //        fanfics->AssignQueuedForFic(fic);

    auto genreLists = authors->GetListGenreData();
    qDebug() << "got genre lists, size: " << genreLists.size();

    auto genresForFics = fanfics->GetGenreForFics();
    qDebug() << "collected genres for fics, size: " << genresForFics.size();
    auto moodLists = authors->GetMoodDataForLists();
    qDebug() << "got mood lists, size: " << moodLists.size();

    //auto genreRedirects = CreateGenreRedirects();

    QVector<genre_stats::FicGenreData> ficGenreDataList =
            CreateGenreDataForFics({genreLists,
                                    moodLists,
                                    genresForFics,
                                    filteredFicsToUse}, CutoffControls{});

    database::Transaction transaction(db);
    if(!genres->WriteDetectedGenres(ficGenreDataList))
        transaction.cancel();

    qDebug() << "finished writing genre data for fics";
    transaction.finalize();
    qDebug() << "Starting the second iteration";

}


//        int64_t pureDrama = std::count_if(genreLists[fic].begin(), genreLists[fic].end(), [&](int listId){
//            return genreLists[listId][static_cast<size_t>(genreRedirects["Drama"])] - genreLists[listId][static_cast<size_t>(genreRedirects["Romance"])] >= 0.05;
//        });
//        int64_t pureRomance = std::count_if(genreLists[fic].begin(), genreLists[fic].end(), [&](int listId){
//            return genreLists[listId][static_cast<size_t>(genreRedirects["Romance"])] - genreLists[listId][static_cast<size_t>(genreRedirects["Drama"])] >= 0.8;
//        });

struct InputForGenresIteration2{
    typedef core::DataHolderInfo<core::rdt_favourites>::type FavType;
    typedef core::DataHolderInfo<core::rdt_fic_genres_composite>::type FicGenreCompositeType;
    typedef core::DataHolderInfo<core::rdt_fic_genres_original>::type FicGenreOriginalType;

    FavType faves;
    FicGenreCompositeType ficGenresComposite;
    FicGenreOriginalType ficGenresOriginal;


    FicGenreCompositeType filteredFicGenresComposite;
    FicGenreOriginalType filteredFicGenresOriginal;
};

template <core::ERecDataType EnumType, typename ContainerType, typename InterfaceType>
void LoadDataForCalc(InterfaceType interface, ContainerType& container, QString storageFolder, QString suffix = "")
{
    QDir dir(QDir::currentPath());
    dir.mkdir("ServerData");

    QString ptrStr = QString("0x%1").arg((quintptr)&container,
                                         QT_POINTER_SIZE * 2, 16, QChar('0'));
    QLOG_INFO() << "loading with suffix: " << suffix << " to: " << ptrStr;

    QString fileBase;
    if(suffix.isEmpty())
        fileBase = QString::fromStdString(core::DataHolderInfo<EnumType>::fileBase());
    else
        fileBase = QString::fromStdString(core::DataHolderInfo<EnumType>::fileBase(suffix));

    QSettings settings("settings_server.ini", QSettings::IniFormat);
    if(settings.value("Settings/usestoreddata", false).toBool() && QFile::exists(storageFolder + "/" +
                                                                                 fileBase
                                                                                 + "_0.txt"))
    {
        thread_boost::LoadData(storageFolder,
                               fileBase,
                               container);
    }
    else
    {
        container = core::DataHolderInfo<EnumType>::loadFunc()(interface);
        thread_boost::SaveData(storageFolder,
                               fileBase,
                               container);
    }
}

template <core::ERecDataType EnumType, typename ContainerType>
void SaveDataForCalc(ContainerType& container, QString storageFolder, QString suffix = "")
{
    QString fileBase;
    if(suffix.isEmpty())
        fileBase = QString::fromStdString(core::DataHolderInfo<EnumType>::fileBase());
    else
        fileBase = QString::fromStdString(core::DataHolderInfo<EnumType>::fileBase(suffix));
    QDir dir(QDir::currentPath());
    if(fileBase.isEmpty())
        return;
    //    dir.remove(fileBase + "*");
    thread_boost::SaveData(storageFolder,
                           fileBase,
                           container);
}


void ServitorWindow::CreateAdjustedGenresForAuthors()
{
    InputForGenresIteration2 inputs;


    interfaces::GenreConverter converter;

    QVector<int> ficIds;
    auto db = QSqlDatabase::database();
    auto genres  = QSharedPointer<interfaces::Genres> (new interfaces::Genres());
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    auto authors= QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    genres->db = db;
    fanfics->db = db;
    authors->db = db;

    LoadDataForCalc<core::rdt_fic_genres_composite,
            InputForGenresIteration2::FicGenreCompositeType,
            QSharedPointer<interfaces::Genres>>(genres, inputs.ficGenresComposite, "TempData");
    LoadDataForCalc<core::rdt_favourites,
            InputForGenresIteration2::FavType,
            QSharedPointer<interfaces::Authors>>(authors, inputs.faves, "TempData");

    auto& faves  = inputs.faves;
    QLOG_INFO() << "Loaded faves of size: " << inputs.faves.size();

    //return;
    authorGenreDataOriginal = authors->GetListGenreData();
    SaveDataForCalc<core::rdt_author_genre_distribution,
            QHash<int, std::array<double, 22>>>(authorGenreDataOriginal, "TempData", "original");


    qDebug() << "Starting the second iteration";
    iteratorProcessor.ReprocessGenreStats(inputs.ficGenresComposite, faves);
    SaveDataForCalc<core::rdt_author_genre_distribution,
            QHash<int, std::array<double, 22>>>(iteratorProcessor.resultingGenreAuthorData, "TempData", "adjusted");
    qDebug() << "finished iteration";
    //return;
    QLOG_INFO() << "Size of original author data: " << authorGenreDataOriginal.size();
    QLOG_INFO() << "Size of adjusted author data: " << iteratorProcessor.resultingGenreAuthorData.size();

    originalMoodData = iteratorProcessor.CreateMoodDataFromGenres(authorGenreDataOriginal);
    adjustedMoodData = iteratorProcessor.resultingMoodAuthorData;


    qDebug() << "Finished queue set";
}
void ServitorWindow::CreateSecondIterationOfGenresForFics(int minAuthorRecs, int minFoundLists)
{
    InputForGenresIteration2 inputs;
    QVector<int> ficIds;
    auto db = QSqlDatabase::database();
    auto genres  = QSharedPointer<interfaces::Genres> (new interfaces::Genres());
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    auto authors= QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    genres->db = db;
    fanfics->db = db;
    authors->db = db;

//    LoadDataForCalc<core::rdt_fic_genres_composite,
//            InputForGenresIteration2::FicGenreCompositeType,
//            QSharedPointer<interfaces::Genres>>(genres, inputs.ficGenresComposite, "TempData");
    LoadDataForCalc<core::rdt_favourites,
            InputForGenresIteration2::FavType,
            QSharedPointer<interfaces::Authors>>(authors, inputs.faves, "TempData");
//    LoadDataForCalc<core::rdt_fic_genres_original,
//            InputForGenresIteration2::FicGenreOriginalType,
//            QSharedPointer<interfaces::Fanfics>>(fanfics, inputs.ficGenresOriginal, "TempData");



    QHash<int, QSet<int>> ficsToUse;
    auto& faves  = inputs.faves;

    for(int key : faves.keys())
    {
        auto& set = faves[key];
        if(set.cardinality() < minAuthorRecs)
            continue;
        for(auto fic : set)
            ficsToUse[fic].insert(key);
    }
    qDebug() << "Finished author processing, resulting set is of size:" << ficsToUse.size();

    QHash<int, QSet<int>> filteredFicsToUse;
    for(auto key : ficsToUse.keys())
    {
        if(ficsToUse[key].size() >= minFoundLists)
        {
            filteredFicsToUse[key] = ficsToUse[key];
            inputs.filteredFicGenresOriginal[key] = inputs.ficGenresOriginal[key];
            inputs.filteredFicGenresComposite[key] = inputs.ficGenresComposite[key];
        }
    }
    QLOG_INFO() << "////////////// ORIGINAL DATA /////////////////";

    QVector<genre_stats::FicGenreData> ficGenreDataList = CreateGenreDataForFics({authorGenreDataOriginal,
                                                                                  originalMoodData,
                                                                                  inputs.ficGenresOriginal,
                                                                                  filteredFicsToUse}, CutoffControls{}, false, ui->chkLogFic->isChecked());
    QLOG_INFO() << "////////////// ADJUSTED DATA /////////////////";
    CutoffControls customCutoff;
    customCutoff.funny = 0.15;
    customCutoff.adventure = 0.2;
    customCutoff.flirty = 0.45;
    customCutoff.drama = 0.2;
    customCutoff.hurty = 0.12;
    customCutoff.bonds = 0.3;
    ficGenreDataList = CreateGenreDataForFics({iteratorProcessor.resultingGenreAuthorData,
                                                                                  adjustedMoodData,
                                                                                  inputs.ficGenresOriginal,
                                                                                  filteredFicsToUse}, customCutoff ,false, ui->chkLogFic->isChecked());
    database::Transaction transaction(db);

    if(!genres->WriteDetectedGenresIteration2(ficGenreDataList))
        transaction.cancel();
    else
        transaction.finalize();
    qDebug() << "finished writing genre data for fics";

}
void ServitorWindow::on_pbLoadGenreDistributions_clicked()
{
    auto db = QSqlDatabase::database();
    auto genres  = QSharedPointer<interfaces::Genres> (new interfaces::Genres());
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    auto authors= QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    genres->db = db;
    fanfics->db = db;
    authors->db = db;
    authorGenreDataOriginal.clear();
    iteratorProcessor.resultingGenreAuthorData.clear();

    LoadDataForCalc<core::rdt_author_genre_distribution,
            QHash<int, std::array<double, 22>>,
            QSharedPointer<interfaces::Authors>>(authors, iteratorProcessor.resultingGenreAuthorData, "TempData", "adjusted");

    LoadDataForCalc<core::rdt_author_genre_distribution,
            QHash<int, std::array<double, 22>>,
            QSharedPointer<interfaces::Authors>>(authors, authorGenreDataOriginal, "TempData", "original");
    originalMoodData = iteratorProcessor.CreateMoodDataFromGenres(authorGenreDataOriginal);
    adjustedMoodData = iteratorProcessor.CreateMoodDataFromGenres(iteratorProcessor.resultingGenreAuthorData);

}

void ServitorWindow::on_pbLoadFic_clicked()
{
    //    PageManager pager;
    //    FicParser parser;
    //    QHash<QString, int> fandoms;
    //    auto result = database::GetAllFandoms(fandoms);
    //    if(!result)
    //        return;
    //    QString url = ui->leFicUrl->text();
    //    auto page = pager.GetPage(url, ECacheMode::use_cache);
    //    parser.SetRewriteAuthorNames(false);
    //    parser.ProcessPage(url, page.content);
    //    parser.WriteProcessed(fandoms);
}

void ServitorWindow::on_pbReprocessFics_clicked()
{
    //    PageManager pager;
    //    FicParser parser;
    //    QHash<QString, int> fandoms;
    //    auto result = database::GetAllFandoms(fandoms);
    //    if(!result)
    //        return;
    //    QSqlDatabase db = QSqlDatabase::database();
    //    //db.transaction();
    //    database::ReprocessFics(" where fandom1 like '% CROSSOVER' and alive = 1 order by id asc", "ffn", [this,&pager, &parser, &fandoms](int ficId){
    //        //todo, get web_id from fic_id
    //        QString url = url_utils::GetUrlFromWebId(webId, "ffn");
    //        auto page = pager.GetPage(url, ECacheMode::use_only_cache);
    //        parser.SetRewriteAuthorNames(false);
    //        auto fic = parser.ProcessPage(url, page.content);
    //        if(fic.isValid)
    //            parser.WriteProcessed(fandoms);
    //    });
}

void ServitorWindow::on_pushButton_clicked()
{
    //database::EnsureFandomsNormalized();
}

void ServitorWindow::on_pbGetGenresForFic_clicked()
{

}

void ServitorWindow::on_pbGetGenresForEverything_clicked()
{
    DetectGenres(25,15);
}

void ServitorWindow::on_pbGenresIteration2_clicked()
{
    CreateAdjustedGenresForAuthors();
    //25, 15
}

void ServitorWindow::on_pushButton_2_clicked()
{
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanfics->db = QSqlDatabase::database();
    fanfics->ResetActionQueue();
    QSettings settings("settings_servitor.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    settings.setValue("Settings/genrequeued", false);
    settings.sync();
}

void ServitorWindow::on_pbGetData_clicked()
{
    An<PageManager> pager;
    auto data = pager->GetPage(ui->leGetCachedData->text(), ECacheMode::use_only_cache);
    ui->edtLog->clear();
    ui->edtLog->insertPlainText(data.content);

    // need to get:
    // last date of published favourite fic
    // amount of favourite fics at the moment of last parse
    // (need to keep this to check if list is updated even if last published is teh same)
}

void ServitorWindow::on_pushButton_3_clicked()
{
    auto db = QSqlDatabase::database();
    auto authorInterface = QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    authorInterface->db = db;
    auto fanfics = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanfics->db = db;
    auto fandomInterface = QSharedPointer<interfaces::Fandoms> (new interfaces::Fandoms());
    authorInterface->db = db;
    auto authors = authorInterface->GetAllAuthorsLimited("ffn", 0);


    An<PageManager> pager;

    auto job = [fanfics, authorInterface, fandomInterface](QString url, QString content){
        FavouriteStoryParser parser(fanfics);
        parser.ProcessPage(url, content);
        return parser;
    };

    QList<QFuture<FavouriteStoryParser>> futures;
    QList<FavouriteStoryParser> parsers;



    database::Transaction transaction(db);
    WebPage data;
    int counter = 0;
    for(auto author : authors)
    {
        if(counter%1000 == 0)
            QLOG_INFO() <<  counter;

        futures.clear();
        parsers.clear();
        //QLOG_INFO() <<  "Author: " << author->url("ffn");
        FavouriteStoryParser parser(fanfics);
        parser.authorName = author->name;

        //TimedAction pageAction("Page loaded in: ",[&](){
        data = pager->GetPage(author->url("ffn"), ECacheMode::use_only_cache);
        //});
        //pageAction.run();

        //TimedAction pageProcessAction("Page processed in: ",[&](){
        auto splittings = page_utils::SplitJob(data.content);

        for(auto part: splittings.parts)
        {
            futures.push_back(QtConcurrent::run(job, data.url, part.data));
        }
        for(auto future: futures)
        {
            future.waitForFinished();
            parsers+=future.result();
        }

        //});
        //pageProcessAction.run();

        QSet<QString> fandoms;
        FavouriteStoryParser::MergeStats(author, fandomInterface, parsers);
        //TimedAction action("Author updated in: ",[&](){
        authorInterface->UpdateAuthorFavouritesUpdateDate(author);
        //});
        //action.run();
        //QLOG_INFO() <<  "Author: " << author->url("ffn") << " Update date: " << author->stats.favouritesLastUpdated;
        counter++;

    }
    transaction.finalize();

}

void ServitorWindow::on_pbUpdateFreshAuthors_clicked()
{
    auto db = QSqlDatabase::database();
    auto authorInterface = QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    authorInterface->db = db;
    authorInterface->portableDBInterface = dbInterface;

    auto authors = authorInterface->GetAllAuthorsWithFavUpdateSince("ffn", QDateTime::currentDateTime().addMonths(-24));

    auto fandomInterface = QSharedPointer<interfaces::Fandoms> (new interfaces::Fandoms());
    fandomInterface->db = db;
    fandomInterface->portableDBInterface = dbInterface;

    auto fanficsInterface = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanficsInterface->db = db;
    fanficsInterface->authorInterface = authorInterface;
    fanficsInterface->fandomInterface = fandomInterface;

    auto recsInterface = QSharedPointer<interfaces::RecommendationLists> (new interfaces::RecommendationLists());
    recsInterface->db = db;
    recsInterface->portableDBInterface = dbInterface;
    recsInterface->authorInterface = authorInterface;



    RecommendationsProcessor reloader(db, fanficsInterface,
                                      fandomInterface,
                                      authorInterface,
                                      recsInterface);

    connect(&reloader, &RecommendationsProcessor::resetEditorText, this,    &ServitorWindow::OnResetTextEditor);
    connect(&reloader, &RecommendationsProcessor::requestProgressbar, this, &ServitorWindow::OnProgressBarRequested);
    connect(&reloader, &RecommendationsProcessor::updateCounter, this,      &ServitorWindow::OnUpdatedProgressValue);
    connect(&reloader, &RecommendationsProcessor::updateInfo, this,         &ServitorWindow::OnNewProgressString);


    reloader.SetStagedAuthors(authors);
    reloader.ReloadRecommendationsList(ECacheMode::use_cache);

}

void ServitorWindow::OnResetTextEditor()
{

}

void ServitorWindow::OnProgressBarRequested()
{

}

void ServitorWindow::OnUpdatedProgressValue(int )
{

}

void ServitorWindow::OnNewProgressString(QString )
{

}

void ServitorWindow::on_pbUnpdateInterval_clicked()
{
    auto db = QSqlDatabase::database();
    auto authorInterface = QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    authorInterface->db = db;
    authorInterface->portableDBInterface = dbInterface;

    auto authors = authorInterface->GetAllAuthorsWithFavUpdateBetween("ffn",
                                                                      QDateTime::currentDateTime().addMonths(-1*ui->sbUpdateIntervalStart->value()),
                                                                      QDateTime::currentDateTime().addMonths(-1*ui->sbUpdateIntervalEnd->value())

                                                                      );



    auto fandomInterface = QSharedPointer<interfaces::Fandoms> (new interfaces::Fandoms());
    fandomInterface->db = db;
    fandomInterface->portableDBInterface = dbInterface;

    auto fanficsInterface = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanficsInterface->db = db;
    fanficsInterface->authorInterface = authorInterface;
    fanficsInterface->fandomInterface = fandomInterface;

    auto recsInterface = QSharedPointer<interfaces::RecommendationLists> (new interfaces::RecommendationLists());
    recsInterface->db = db;
    recsInterface->portableDBInterface = dbInterface;
    recsInterface->authorInterface = authorInterface;



    RecommendationsProcessor reloader(db, fanficsInterface,
                                      fandomInterface,
                                      authorInterface,
                                      recsInterface);

    connect(&reloader, &RecommendationsProcessor::resetEditorText, this,    &ServitorWindow::OnResetTextEditor);
    connect(&reloader, &RecommendationsProcessor::requestProgressbar, this, &ServitorWindow::OnProgressBarRequested);
    connect(&reloader, &RecommendationsProcessor::updateCounter, this,      &ServitorWindow::OnUpdatedProgressValue);
    connect(&reloader, &RecommendationsProcessor::updateInfo, this,         &ServitorWindow::OnNewProgressString);


    reloader.SetStagedAuthors(authors);
    reloader.ReloadRecommendationsList(ECacheMode::use_cache);
}

void ServitorWindow::on_pbReprocessAllFavPages_clicked()
{
    auto db = QSqlDatabase::database();
    auto authorInterface = QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    authorInterface->db = db;
    authorInterface->portableDBInterface = dbInterface;

    auto authors = authorInterface->GetAllAuthors("ffn");


    auto fandomInterface = QSharedPointer<interfaces::Fandoms> (new interfaces::Fandoms());
    fandomInterface->db = db;
    fandomInterface->portableDBInterface = dbInterface;

    auto fanficsInterface = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanficsInterface->db = db;
    fanficsInterface->authorInterface = authorInterface;
    fanficsInterface->fandomInterface = fandomInterface;

    auto recsInterface = QSharedPointer<interfaces::RecommendationLists> (new interfaces::RecommendationLists());
    recsInterface->db = db;
    recsInterface->portableDBInterface = dbInterface;
    recsInterface->authorInterface = authorInterface;



    RecommendationsProcessor reloader(db, fanficsInterface,
                                      fandomInterface,
                                      authorInterface,
                                      recsInterface);

    connect(&reloader, &RecommendationsProcessor::resetEditorText, this,    &ServitorWindow::OnResetTextEditor);
    connect(&reloader, &RecommendationsProcessor::requestProgressbar, this, &ServitorWindow::OnProgressBarRequested);
    connect(&reloader, &RecommendationsProcessor::updateCounter, this,      &ServitorWindow::OnUpdatedProgressValue);
    connect(&reloader, &RecommendationsProcessor::updateInfo, this,         &ServitorWindow::OnNewProgressString);


    reloader.SetStagedAuthors(authors);
    reloader.ReloadRecommendationsList(ECacheMode::use_only_cache);
    authorInterface->AssignAuthorNamesForWebIDsInFanficTable();
}

void ServitorWindow::on_pbGetNewFavourites_clicked()
{
    if(!env.ResumeUnfinishedTasks())
        env.LoadAllLinkedAuthors(ECacheMode::use_cache);
}

void ServitorWindow::on_pbReprocessCacheLinked_clicked()
{
    env.LoadAllLinkedAuthorsMultiFromCache();
}

void ServitorWindow::on_pbPCRescue_clicked()
{
    QString path = "PageCache.sqlite";
    QSqlDatabase pcdb = QSqlDatabase::addDatabase("QSQLITE", "PageCache");
    pcdb.setDatabaseName(path);
    pcdb.open();


    path = "PageCache_export.sqlite";
    QSqlDatabase pcExDb = QSqlDatabase::addDatabase("QSQLITE", "PageCache_Export");
    pcExDb.setDatabaseName(path);
    pcExDb.open();


    QSharedPointer<database::IDBWrapper> pageCacheInterface (new database::SqliteInterface());
    QSharedPointer<database::IDBWrapper> pageCacheExportInterface (new database::SqliteInterface());

    pcdb = pageCacheInterface->InitDatabase("PageCache");
    qDebug() << "Db open: " << pcdb.isOpen();
    QSqlQuery testQuery("select * from pagecache where url = 'https://www.fanfiction.net/u/1000039'", pcdb);
    bool readable = testQuery.next();
    qDebug() << "Readable: " << readable;
    qDebug() << "Data: " << testQuery.record();

    pcExDb = pageCacheExportInterface->InitDatabase("PageCache_Export");

    pageCacheInterface->ReadDbFile("dbcode/pagecacheinit.sql", "PageCache");
    pageCacheExportInterface->ReadDbFile("dbcode/pagecacheinit.sql", "PageCache_Export");


    int counter = 0;

    QString insert = "INSERT INTO PAGECACHE(URL, GENERATION_DATE, CONTENT,  PAGE_TYPE, COMPRESSED) "
                     "VALUES(:URL, :GENERATION_DATE, :CONTENT, :PAGE_TYPE, :COMPRESSED)";
    database::Transaction transactionMain(env.interfaces.db->GetDatabase());
    //database::Transaction transactionRead(pcdb);
    QSqlQuery exportQ(pcExDb);
    exportQ.prepare(insert);
    auto authors = env.interfaces.authors->GetAllAuthorsUrls("ffn");
    std::sort(authors.begin(), authors.end());
    qDebug() << "finished reading author urls";
    QSqlQuery readQuery(pcdb);
    readQuery.prepare("select * from pagecache where url = :url");

    database::Transaction transaction(pcExDb);

    for(auto author : authors)
    {
        //qDebug() << "Attempting to read url: " << author;
        readQuery.bindValue(":url", author);
        readQuery.exec();
        bool result = readQuery.next();
        if(!result || readQuery.value("url").toString().isEmpty())
        {
            qDebug() << "Attempting to read url: " << author;
            qDebug() << readQuery.lastError().text();
            continue;
        }
        if(counter%1000 == 0)
        {
            qDebug() << "committing: " << counter;
            transaction.finalize();
            transaction.start();
        }

        //qDebug() << "writing record: " << readQuery.value("url").toString();

        counter++;
        exportQ.bindValue(":URL", readQuery.value("url").toString());
        exportQ.bindValue(":GENERATION_DATE", readQuery.value("GENERATION_DATE").toDateTime());
        exportQ.bindValue(":CONTENT", readQuery.value("CONTENT").toByteArray());
        exportQ.bindValue(":COMPRESSED", readQuery.value("COMPRESSED").toInt());
        exportQ.bindValue(":PAGE_TYPE", readQuery.value("PAGE_TYPE").toString());
        exportQ.exec();
        if(exportQ.lastError().isValid())
            qDebug() << "Error writing record: " << exportQ.lastError().text();
    }


}

void ServitorWindow::on_pbSlashCalc_clicked()
{
    auto db = QSqlDatabase::database();
    database::Transaction transaction(db);
    auto authorInterface = QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    authorInterface->db = db;
    authorInterface->portableDBInterface = dbInterface;

    auto authors = authorInterface->GetAllAuthors("ffn");


    auto fandomInterface = QSharedPointer<interfaces::Fandoms> (new interfaces::Fandoms());
    fandomInterface->db = db;
    fandomInterface->portableDBInterface = dbInterface;

    auto fanficsInterface = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    fanficsInterface->db = db;
    fanficsInterface->authorInterface = authorInterface;
    fanficsInterface->fandomInterface = fandomInterface;

    auto recsInterface = QSharedPointer<interfaces::RecommendationLists> (new interfaces::RecommendationLists());
    recsInterface->db = db;
    recsInterface->portableDBInterface = dbInterface;
    recsInterface->authorInterface = authorInterface;

    SlashProcessor slash(db,fanficsInterface, fandomInterface, authorInterface, recsInterface, dbInterface);
    slash.DoFullCycle(db, 2);
    transaction.finalize();

}

void ServitorWindow::on_pbFindSlashSummary_clicked()
{
    CommonRegex rx;
    rx.Init();
    auto result = rx.ContainsSlash("A year after his mother's death, Ichigo finds himself in a world that he knows doesn't exist and met four spirits. "
                                   "Deciding to know what they truly are, he goes to his father who takes him to a shop. "
                                   "Take a different road IchiHime fans. Dual Wield Zanpakuto! Resurreccion! "
                                   "Quincy powers! OOC. New chapters every week or two. HIATUS",
                                   "[Ichigo K., Yoruichi S.] Rukia K., T. Harribel",
                                   "Bleach");
}

void ServitorWindow::LoadDataForCalculation(CalcDataHolder& cdh)
{
    auto db = QSqlDatabase::database();
    auto genresInterface  = QSharedPointer<interfaces::Genres> (new interfaces::Genres());
    auto fanficsInterface = QSharedPointer<interfaces::Fanfics> (new interfaces::FFNFanfics());
    auto authorsInterface= QSharedPointer<interfaces::Authors> (new interfaces::FFNAuthors());
    genresInterface->db = db;
    fanficsInterface->db = db;
    authorsInterface->db = db;

    database::Transaction transaction(db);


    if(!QFile::exists("TempData/fandomstats_0.txt"))
    {
        TimedAction action("Loading data",[&](){
            cdh.fics = env.interfaces.fanfics->GetAllFicsWithEnoughFavesForWeights(ui->sbFicCount->value());
            cdh.favourites = authorsInterface->LoadFullFavouritesHashset();
            cdh.genreData = authorsInterface->GetListGenreData();
            qDebug() << "got genre lists, size: " << cdh.genreData.size();
            cdh.fandomLists = authorsInterface->GetAuthorListFandomStatistics(cdh.favourites.keys());
            qDebug() << "got fandom lists, size: " << cdh.fandomLists.size();
            cdh.authors = authorsInterface->GetAllAuthors("ffn");
        });
        action.run();

        TimedAction saveAction("Saving data",[&](){
            cdh.SaveFicsData();
        });
        saveAction.run();
    }
    else
    {
        TimedAction loadAction("Loading data",[&](){
            cdh.LoadFicsData();
        });
        loadAction.run();
    }
    transaction.finalize();
    QSet<int> ficSet;
    for(auto fic : cdh.fics)
        ficSet.insert(fic->id);

    qDebug() << "ficset is of size:" << ficSet.size();

    // need to reduce fav sets and remove every fic that isn't in the calculation
    // to reduce throttling
    // perhaps doesn't need to be a set, a vector might do
    for(auto& favSet : cdh.favourites)
    {
        int previousSize = favSet.size();
        favSet.intersect(ficSet);
        int newSize = favSet.size();
        qDebug() << "P: " << previousSize << "N: " << newSize;
    }

    auto it = cdh.favourites.begin();
    auto end = cdh.favourites.end();
    while(it != end)
    {
        if(it.value().size() < 1200)
            cdh.filteredFavourites.push_back({it.key(), it.value()});
        it++;
    }

    std::sort(cdh.filteredFavourites.begin(), cdh.filteredFavourites.end(), [](const ListWithIdentifier& fp1, const ListWithIdentifier& fp2){
        return fp1.favourites.size() < fp2.favourites.size();
    });
    for(auto& list: cdh.filteredFavourites)
        qDebug() << "Size: " << list.favourites.size();

}

struct FicPair{
    //    /uint32_t count = 0;
    //    float val1;
    //    float val2;
    //    float val3;
    //    float val4;
    //    float val5;
    Roaring r;
};

//struct SmartHash{
//    void CleanTemporaryStorage();
//    void PrepareForList(const QList<QPair<uint32_t, uint32_t>>& list);

//    QHash<QPair<uint32_t, uint32_t>, Roaring> workingSet;
//};


void ServitorWindow::ProcessCDHData(CalcDataHolder& cdh){

    for(auto fav : cdh.filteredFavourites)
    {
        for(auto fic : fav.favourites)
        {
            ficsToFavLists[fic].setCopyOnWrite(true);
            ficsToFavLists[fic].add(fav.id);
        }
    }

    for(auto fic : cdh.fics)
    {
        if(!ficsToFavLists.contains(fic->id))
            continue;
        ficData[fic->id] = fic;
        for(auto fandom : fic->fandoms)
            ficsForFandoms[fandom].insert(fic->id);
    }
    qDebug() << "Will work with: " << ficData.size() << " fics";
    qDebug() << 1;

    qDebug() << 2;
}

struct Sink{
    Sink(){}
    template <typename T>
    void SaveIntersection(uint32_t fic1,uint32_t fic2, const T& intersection){
        //        QVector<uint32_t> vec;
        //        for(auto item : intersection)
        //            vec.push_back(item);
        //        std::sort(vec.begin(), vec.end());
        //        qDebug() << fic1 << "  " << fic2;
        //        qDebug() << vec;
        //QWriteLocker locker(&lock);
        counter++;
    }
    std::atomic<int> counter = 0;
    QHash<QPair<uint32_t,uint32_t>, QVector<int>> intersections;
    QReadWriteLock lock;
};

void ServitorWindow::CalcConstantMemory()
{
    CalcDataHolder cdh;
    LoadDataForCalculation(cdh);
    ProcessCDHData(cdh);
    Sink sink;
    keys = ficData.keys();
    auto rng = std::default_random_engine {};
    std::shuffle(std::begin(keys), std::end(keys), rng);
    auto worker = [&](int start, int end, int otherEnd, auto* saver)->void
    {

        for(int i = start; i < end; i++)
        {
            //            if(i > start)
            //                break;
            if(i%100 == 0)
                qDebug() << "working from: " << start << " at: " << i;
            auto fic1 = keys[i];
            for(int j = i+1; j < otherEnd; j++)
            {
                //                if(j > i+1)
                //                    break;
                auto fic2 = keys[j];
                const auto& set1 = ficsToFavLists[fic1];
                const auto& set2 = ficsToFavLists[fic2];
                auto temp = set1;
                temp = temp & set2;

                ///
                //                QVector<uint32_t> vec;
                //                for(auto item : set1)
                //                    vec.push_back(item);
                //                std::sort(vec.begin(), vec.end());
                //                qDebug() << vec;

                //                vec.clear();
                //                for(auto item : set2)
                //                    vec.push_back(item);
                //                std::sort(vec.begin(), vec.end());
                //                qDebug() << vec;

                //                vec.clear();
                //                for(auto item : temp)
                //                    vec.push_back(item);
                //                std::sort(vec.begin(), vec.end());
                //                qDebug() << vec;
                ///

                if(!temp.isEmpty())
                    saver->SaveIntersection(fic1, fic2, temp);
            }
        }
    };

    QList<QFuture<void>> futures;
    //int threads = 1;//QThread::idealThreadCount()-1;
    int threads = QThread::idealThreadCount()-1;
    for(int i = 0; i < threads; i++)
    {
        int partSize = keys.size() / threads;
        int start = i*partSize;
        int end = i == (threads - 1) ? keys.size() : (i+1)*partSize;
        futures.push_back(QtConcurrent::run(worker,
                                            start,
                                            end,
                                            keys.size(),
                                            &sink));
    }
    TimedAction action("processing data",[&](){
        for(auto future: futures)
        {
            future.waitForFinished();
        }
    });
    action.run();
    qDebug() << "Final count:" << sink.counter;
}

void ServitorWindow::on_pbCalcWeights_clicked()
{


    //    CalcConstantMemory();
    //    return;
    CalcDataHolder cdh;
    LoadDataForCalculation(cdh);
    ProcessCDHData(cdh);
    QHash<int, core::FicWeightResult> result;
    QSet<int> alreadyProcessed;

    QHash<QPair<int, int>, QSet<int>> meetingSet;
    QHash<QPair<uint32_t, uint32_t>, FicPair> ficsMeeting;
    QHash<QPair<uint32_t, uint32_t>, FicPair>::iterator ficsIterator;
    QHash<QPair<int, int>, QSet<int>>::iterator meetingIterator;
    QHash<QPair<uint32_t, uint32_t>, Roaring> roaringSet;
    QHash<QPair<uint32_t, uint32_t>, Roaring>::iterator roaringIterator;


    //            {
    //                auto ficIds = ficData.keys();
    //                for(int i = 0; i < ficIds.size(); i++)
    //                {
    //                    if(i%10 == 0)
    //                        qDebug() << "working at: " << i;
    //                    auto fic1 = ficIds[i];
    //                    for(int j = i+1; j < ficIds.size(); j++)
    //                    {
    //                        auto fic2 = ficIds[i];
    //                        auto keys = cdh.favourites.keys();
    //                        QVector<int> intersection;
    //                        auto& set1 = ficsToFavLists[fic1];
    //                        auto& set2 = ficsToFavLists[fic2];
    //                        intersection.reserve(std::max(set1.size(), set2.size()));
    //                        std::set_intersection(set1.begin(), set1.end(),
    //                                              set2.begin(), set2.end(),
    //                                               std::back_inserter(intersection));
    //                        ficsMeeting[{fic1, fic2}].count = intersection.size();
    //                    }
    //                }
    //            }
    // need to create limited sets of unique pairs
    //    QList<QPair<int, int>> pairList;
    //    {
    //        auto ficIds = ficData.keys();
    //        for(int i = 0; i < ficIds.size(); i++)
    //        {
    //            for(int j = i+1; j < ficIds.size(); j++)
    //            {
    //                pairList.push_back({ficIds[i], ficIds[j]});
    //            }
    //        }
    //    }

    {
        int counter = 0;
        QPair<uint32_t, uint32_t> pair;
        qDebug() << "full fav size: " << cdh.filteredFavourites.size();
        for(auto fav : cdh.filteredFavourites)
        {
            auto values = fav.favourites.toList();
            if(counter%100 == 0)
                qDebug() << " At: " << counter << " size: " << values.size();
            counter++;
            if(counter > 213000 && counter%1000 == 0)
                ficsMeeting.squeeze();

            //qDebug() << "Reading list of size: " << values.size();
            for(int i = 0; i < values.size(); i++)
            {
                for(int j = i+1; j < values.size(); j++)
                {
                    uint32_t fic1 = values[i];
                    uint32_t fic2 = values[j];

                    //QPair<uint32_t, uint32_t> pair (std::min(fic1,fic2), std::max(fic1,fic2));
                    pair.first = std::min(fic1,fic2);
                    pair.second = std::max(fic1,fic2);
                    //QPair<uint32_t, uint32_t> pair2 = {std::min(fic1,fic2), std::max(fic1,fic2)};
                    //qDebug() << pair;
                    //qDebug() << pair2;
                    //roaring attempt
                    //                    roaringIterator = roaringSet.find(pair);
                    //                    if(roaringIterator == roaringSet.end())
                    //                    {
                    //                        roaringSet[pair];
                    //                        roaringIterator = roaringSet.find(pair);
                    //                    }

                    //                    roaringIterator->add(fav.id);
                    //                    roaringIterator->shrinkToFit();
                    //meeting attempt
                    //                    meetingIterator = meetingSet.find(pair);
                    //                    if(meetingIterator == meetingSet.end())
                    //                    {
                    //                        meetingSet[pair];
                    //                        meetingIterator = meetingSet.find(pair);
                    //                    }

                    //                    meetingIterator->insert(key);
                    //valueattempt
                    ficsIterator = ficsMeeting.find(pair);
                    if(ficsIterator == ficsMeeting.end())
                    {
                        ficsMeeting[pair];
                        ficsIterator = ficsMeeting.find(pair);
                    }

                    //ficsIterator.value().count++;
                    ficsIterator.value().r.add(fav.id);
                    if(counter > 213000)
                        ficsIterator.value().r.shrinkToFit();
                    //                    pairCounter++;
                }
            }
            //qDebug() << "List had: " << pairCounter << " pairs";
        }
    }
    int countPairs = 0;
    int countListRecords = 0;

    for(auto edge: ficsMeeting)
    {
        countPairs++;
        countListRecords+=edge.r.cardinality();
    }
    qDebug() << "edge count: " << countPairs;
    qDebug() << "list records count: " << countListRecords;
    //    auto values = ficsMeeting.values();
    //    std::sort(values.begin(), values.end(), [](const FicPair& fp1, const FicPair& fp2){
    //        return fp1.count > fp2.count;
    //    });
    //    QString prototype = "SELECT * FROM fanfics where  id in (%1, %2)";
    //    for(int i = 0; i < 10; i++)
    //    {
    //        QString temp = prototype;
    //        //qDebug() << << values[i].fic1 << " " << values[i].fic2 << " " << values[i].meetings.size();
    //        qDebug().noquote() << temp.arg(values[i].fic1) .arg(values[i].fic2);
    //        qDebug() << "Met times: " << values[i].count;
    //        qDebug() << " ";
    //    }

    //fanficsInterface->WriteFicRelations(result);
}


void ServitorWindow::on_pbCleanPrecalc_clicked()
{
    QFile::remove("ficsdata.txt");
}


void ServitorWindow::on_cbGenres_currentIndexChanged(const QString &arg1)
{
    InitGenreChartView(arg1);
}

void ServitorWindow::on_cbMoodSelector_currentTextChanged(const QString &arg1)
{
    InitMoodChartView(arg1);
}

void ServitorWindow::on_pbCalcFicGenres_clicked()
{
    CreateSecondIterationOfGenresForFics(25, 15);
}
