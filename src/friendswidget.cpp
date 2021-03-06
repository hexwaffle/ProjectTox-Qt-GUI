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

#include "addfrienddialog.hpp"
#include "friendproxymodel.hpp"
#include "friendswidget.hpp"

#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QHeaderView>
#include <QVBoxLayout>

FriendsWidget::FriendsWidget(QWidget* parent) :
    QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    layout->setContentsMargins(2, 2, 0, 2);

    QAction* copyUserIdAction = new QAction(QIcon(":/icons/page_copy.png"), "Copy User ID", this);
    connect(copyUserIdAction, &QAction::triggered, this, &FriendsWidget::onCopyUserIdActionTriggered);

    QAction* renameFriendAction = new QAction(QIcon(":/icons/textfield_rename.png"), "Rename", this);
    connect(renameFriendAction, &QAction::triggered, this, &FriendsWidget::onRenameFriendActionTriggered);

    QAction* removeFriendAction = new QAction(QIcon(":/icons/user_delete.png"), "Remove", this);
    connect(removeFriendAction, &QAction::triggered, this, &FriendsWidget::onRemoveFriendActionTriggered);

    friendContextMenu = new QMenu(this);
    friendContextMenu->addActions(QList<QAction*>() << copyUserIdAction << renameFriendAction << removeFriendAction);

    friendView = new CustomHintTreeView(this, QSize(100, 100));
    friendView->setIconSize(QSize(24, 24));
    friendView->setSortingEnabled(true);
    friendView->setIndentation(0);
    friendView->setHeaderHidden(true);
    friendView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    friendView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(friendView, &QTreeView::customContextMenuRequested, this, &FriendsWidget::onFriendContextMenuRequested);

    friendModel = new QStandardItemModel(this);
    connect(friendModel, &QStandardItemModel::itemChanged, this, &FriendsWidget::onFriendModified);

    friendProxyModel = new FriendProxyModel(this);
    friendProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    friendProxyModel->setSourceModel(friendModel);

    friendView->setModel(friendProxyModel);
    connect(friendView->selectionModel(), &QItemSelectionModel::currentChanged, this, &FriendsWidget::onFriendSelectionChanged);


    filterEdit = new FilterWidget(this);
    filterEdit->setPlaceholderText("Search");
    connect(filterEdit, &FilterWidget::textChanged, friendProxyModel, &FriendProxyModel::setFilterFixedString);

    addFriendButton = new QPushButton(QIcon(":/icons/user_add.png"), "Add Friend", this);
    connect(addFriendButton, &QPushButton::clicked, this, &FriendsWidget::onAddFriendButtonClicked);

    layout->addWidget(filterEdit);
    layout->addWidget(friendView);
    layout->addWidget(addFriendButton);
}

void FriendsWidget::addFriend(const QString& userId, const QString& username)
{
    QStandardItem* item = new QStandardItem(username);
    item->setData(userId, UserIdRole);
    item->setData(QString("User ID: %1").arg(userId), Qt::ToolTipRole);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);

    UserInfo info = userInfoHash[userId];
    info.username = username;
    userInfoHash[userId] = info;

    emit friendAdded(userId, username);

    Status status = Status::Offline;
    setStatus(item, status);

    friendModel->appendRow(item);
}

void FriendsWidget::setStatus(const QString& userId, Status status)
{
    QStandardItem* friendItem = findFriendItem(userId);

    if (friendItem == nullptr) {
        return;
    }

    setStatus(friendItem, status);
}

void FriendsWidget::setStatus(QStandardItem* friendItem, Status status)
{
    friendItem->setData(QIcon(StatusHelper::getInfo(status).iconPath), Qt::DecorationRole);

    //used for sorting model
    friendItem->setData(QVariant::fromValue(status), StatusRole);

    QString userId = friendItem->data(UserIdRole).toString();
    UserInfo info = userInfoHash[userId];
    info.status = status;
    userInfoHash[userId] = info;

    emit friendStatusChanged(userId, status);
}

QStandardItem* FriendsWidget::findFriendItem(const QString& userId) const
{
    QModelIndexList indexList = friendModel->match(friendModel->index(0, 0), UserIdRole, userId);
    if (indexList.size() == 1) {
        return friendModel->itemFromIndex(indexList.at(0));
    }

    return nullptr;
}

void FriendsWidget::onAddFriendButtonClicked()
{
    AddFriendDialog dialog(this);

    if (dialog.exec() == QDialog::Accepted) {
        addFriend(dialog.getUserId(), dialog.getUsername());
        emit friendRequested(dialog.getUserId(), dialog.getMessage());
    }
}

FriendsWidget::UserInfo FriendsWidget::getUserInfo(const QString& userId) const
{
    if (userInfoHash.contains(userId)) {
        return userInfoHash[userId];
    }

    return UserInfo();
}

void FriendsWidget::onFriendContextMenuRequested(const QPoint& pos)
{
    QPoint globalPos = friendView->viewport()->mapToGlobal(pos);
    globalPos.setX(globalPos.x() + 1);
    QModelIndexList selectedIndexes = friendView->selectionModel()->selectedIndexes();
    if (selectedIndexes.size() != 1) {
        return;
    }
    friendContextMenu->exec(globalPos);
}

void FriendsWidget::onCopyUserIdActionTriggered()
{
    // friendContextMenuRequested already made sure that there is only one index selected
    QModelIndex selectedIndex = friendView->selectionModel()->selectedIndexes().at(0);
    QGuiApplication::clipboard()->setText(friendProxyModel->mapToSource(selectedIndex).data(UserIdRole).toString());
}

void FriendsWidget::onRenameFriendActionTriggered()
{
    // friendContextMenuRequested already made sure that there is only one index selected
    QModelIndex selectedIndex = friendView->selectionModel()->selectedIndexes().at(0);
    QStandardItem* selectedItem = friendModel->itemFromIndex(friendProxyModel->mapToSource(selectedIndex));
    selectedItem->setFlags(selectedItem->flags() | Qt::ItemIsEditable);
    friendView->edit(selectedIndex);
    selectedItem->setFlags(selectedItem->flags() & ~Qt::ItemIsEditable);
}

// called on any edit of data(...) in the model. status change, username change, etc.
void FriendsWidget::onFriendModified(QStandardItem* item)
{
    QString userId = item->data(UserIdRole).toString();
    QString newUsername = item->text();

    UserInfo info = userInfoHash[userId];
    if (newUsername != info.username) {
        info.username = newUsername;
        userInfoHash[userId] = info;
        emit friendRenamed(userId, newUsername);
    }
}

void FriendsWidget::onRemoveFriendActionTriggered()
{
    QModelIndex selectedIndex = friendView->selectionModel()->selectedIndexes().at(0);
    QList<QStandardItem*> selectedItems = friendModel->takeRow(friendProxyModel->mapToSource(selectedIndex).row());

    QString userId = selectedItems.at(0)->data(UserIdRole).toString();
    userInfoHash.remove(userId);
    emit friendRemoved(userId);

    qDeleteAll(selectedItems);
}
void FriendsWidget::onFriendSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
{
    QStandardItem* item = friendModel->itemFromIndex(friendProxyModel->mapToSource(current));
    if (item != nullptr) {
        emit friendSelectionChanged(item->data(UserIdRole).toString());
    }
}
