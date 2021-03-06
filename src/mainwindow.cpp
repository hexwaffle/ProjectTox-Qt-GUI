/*
    Copyright (C) 2013 by Maxim Biro <nurupo.contributions@gmail.com>
    
    This file is part of Tox Qt GUI.
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    
    See the COPYING file for more details.
*/

#include "mainwindow.hpp"
#include "pageswidget.hpp"
#include "ouruseritemwidget.hpp"
#include "friendrequestdialog.hpp"
#include "dhtdialog.hpp"

#include <QApplication>
#include <QDesktopWidget>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QStackedWidget>

#include <QDebug>

OurUserItemWidget* MainWindow::ourUserItem = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    const int screenWidth = QApplication::desktop()->width();
    const int screenHeight = QApplication::desktop()->height();
    const int appWidth = 500;
    const int appHeight = 300;

    setGeometry((screenWidth - appWidth) / 2, (screenHeight - appHeight) / 2, appWidth, appHeight);

    setWindowTitle("developers' test version, not for public use");

    QDockWidget* friendDock = new QDockWidget(this);
    friendDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    friendDock->setTitleBarWidget(new QWidget(friendDock));
    friendDock->setContextMenuPolicy(Qt::PreventContextMenu);
    addDockWidget(Qt::LeftDockWidgetArea, friendDock);

    QWidget* friendDockWidget = new QWidget(friendDock);
    QVBoxLayout* layout = new QVBoxLayout(friendDockWidget);
    layout->setMargin(0);
    layout->setSpacing(0);

    ourUserItem = new OurUserItemWidget(this);
    friendsWidget = new FriendsWidget(friendDockWidget);

    layout->addWidget(ourUserItem);
    layout->addWidget(friendsWidget);

    friendDock->setWidget(friendDockWidget);

    PagesWidget* pages = new PagesWidget(this);
    connect(friendsWidget, &FriendsWidget::friendAdded, pages, &PagesWidget::addPage);
    connect(friendsWidget, &FriendsWidget::friendRemoved, pages, &PagesWidget::removePage);
    connect(friendsWidget, &FriendsWidget::friendSelectionChanged, pages, &PagesWidget::activatePage);
    connect(friendsWidget, &FriendsWidget::friendRenamed, pages, &PagesWidget::usernameChanged);
    connect(friendsWidget, &FriendsWidget::friendStatusChanged, pages, &PagesWidget::statusChanged);

    DhtDialog dhtDialog(this);
    if (dhtDialog.exec() != QDialog::Accepted) {
        QTimer::singleShot(250, qApp, SLOT(quit()));
    }

    core = new Core(dhtDialog.getUserId(), dhtDialog.getIp(), dhtDialog.getPort());

    coreThread = new QThread(this);
    coreThread->setPriority(QThread::IdlePriority);
    core->moveToThread(coreThread);
    connect(coreThread, &QThread::started, core, &Core::run);
    coreThread->start();
    connect(friendsWidget, &FriendsWidget::friendRequested, core, &Core::requestFriendship);
    connect(core, &Core::friendRequestRecieved, this, &MainWindow::onFriendRequestRecieved);
    connect(this, &MainWindow::friendRequestAccepted, core, &Core::acceptFirendRequest);
    connect(core, &Core::userIdGererated, ourUserItem, &OurUserItemWidget::setUserId);
    connect(core, &Core::friendStatusChanged, this, &MainWindow::onFriendStatusChanged);
    connect(pages, &PagesWidget::messageSent, core, &Core::sendMessage);
    connect(core, &Core::friendMessageRecieved, pages, &PagesWidget::messageReceived);
    connect(friendsWidget, &FriendsWidget::friendRemoved, core, &Core::removeFriend);

    setCentralWidget(pages);
}

MainWindow::~MainWindow()
{
    coreThread->quit();
    coreThread->wait();
    delete core;
}

void MainWindow::sendMessage()
{

}

void MainWindow::onFriendRequestRecieved(const QString &userId, const QString &message)
{
    FriendRequestDialog dialog(this, userId, message);

    if (dialog.exec() == QDialog::Accepted) {
        friendsWidget->addFriend(userId, userId.mid(0, 5));
        emit friendRequestAccepted(userId);
    }
}

void MainWindow::onFriendStatusChanged(const QString &userId, Core::FriendStatus status)
{
    switch (status) {
        case Core::FriendStatus::NotFound:
            qDebug() << "status:" << "no such friend found" << userId;
            friendsWidget->setStatus(userId, Status::Offline);
            break;
        case Core::FriendStatus::Added:
            qDebug() << "status:" << "friend was added" << userId;
            friendsWidget->setStatus(userId, Status::Offline);
            break;
        case Core::FriendStatus::RequestSent:
            qDebug() << "status:" << "friend request was sent" << userId;
            friendsWidget->setStatus(userId, Status::Offline);
            break;
        case Core::FriendStatus::Confirmed:
            qDebug() << "status:" << "friend is confirmed" << userId;
            friendsWidget->setStatus(userId, Status::Offline);
            break;
        case Core::FriendStatus::Online:
            qDebug() << "status:" << "friend is online" << userId;
            friendsWidget->setStatus(userId, Status::Online);
            break;
    }
}
