// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Authors: Orochimarufan <orochimarufan.x3@gmail.com>
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "MinecraftAccount.h"

#include <QCryptographicHash>
#include <QJsonObject>
#include <QUuid>

#include <QDebug>

#include "minecraft/auth/AccountData.h"

MinecraftAccount::MinecraftAccount(QObject* parent) : QObject(parent)
{
    data.internalId = QUuid::createUuid().toString(QUuid::Id128);
}

MinecraftAccountPtr MinecraftAccount::loadFromJsonV3(const QJsonObject& json)
{
    MinecraftAccountPtr account(new MinecraftAccount());
    if (account->data.resumeStateFromV3(json)) {
        return account;
    }
    return nullptr;
}

MinecraftAccountPtr MinecraftAccount::createOffline(const QString& username)
{
    auto account = makeShared<MinecraftAccount>();
    account->data.yggdrasilToken.token = "0";
    account->data.yggdrasilToken.validity = Validity::Certain;
    account->data.yggdrasilToken.issueInstant = QDateTime::currentDateTimeUtc();
    account->data.yggdrasilToken.extra["userName"] = username;
    account->data.yggdrasilToken.extra["clientToken"] = QUuid::createUuid().toString(QUuid::Id128);
    account->data.minecraftProfile.id = uuidFromUsername(username).toString(QUuid::Id128);
    account->data.minecraftProfile.name = username;
    account->data.minecraftProfile.validity = Validity::Certain;
    return account;
}

QJsonObject MinecraftAccount::saveToJson() const
{
    return data.saveState();
}

QString MinecraftAccount::displayName() const
{
    return profileName();
}

void MinecraftAccount::fillSession(AuthSessionPtr session)
{
    // volatile auth token
    session->access_token = data.accessToken();
    // profile name
    session->player_name = data.profileName();
    // profile ID
    session->uuid = data.profileId();
    if (session->uuid.isEmpty())
        session->uuid = uuidFromUsername(session->player_name).toString(QUuid::Id128);
    // always 'offline': there is no authenticated session type
    session->user_type = "offline";
    if (!session->access_token.isEmpty()) {
        session->session = "token:" + data.accessToken() + ":" + data.profileId();
    } else {
        session->session = "-";
    }
}

void MinecraftAccount::decrementUses()
{
    Usable::decrementUses();
    if (!isInUse()) {
        emit changed();
        // FIXME: we now need a better way to identify accounts...
        qWarning() << "Profile" << data.profileId() << "is no longer in use.";
    }
}

void MinecraftAccount::incrementUses()
{
    bool wasInUse = isInUse();
    Usable::incrementUses();
    if (!wasInUse) {
        emit changed();
        // FIXME: we now need a better way to identify accounts...
        qWarning() << "Profile" << data.profileId() << "is now in use.";
    }
}

QUuid MinecraftAccount::uuidFromUsername(QString username)
{
    auto input = QString("OfflinePlayer:%1").arg(username).toUtf8();

    // basically a reimplementation of Java's UUID#nameUUIDFromBytes
    QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Md5);

    auto bOr = [](QByteArray& array, qsizetype index, uint8_t value) { array[index] |= value; };
    auto bAnd = [](QByteArray& array, qsizetype index, uint8_t value) { array[index] &= value; };
    bAnd(digest, 6, 0x0f);  // clear version
    bOr(digest, 6, 0x30);   // set to version 3
    bAnd(digest, 8, 0x3f);  // clear variant
    bOr(digest, 8, 0x80);   // set to IETF variant

    return QUuid::fromRfc4122(digest);
}
