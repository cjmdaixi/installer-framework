/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "remoteserver.h"

#include "protocol.h"
#include "remoteserver_p.h"

namespace QInstaller {

RemoteServer::RemoteServer(QObject *parent)
    : QObject(parent)
    , d_ptr(new RemoteServerPrivate(this))
{
}

RemoteServer::~RemoteServer()
{
    Q_D(RemoteServer);
    d->m_thread.quit();
    d->m_thread.wait();
}

/*!
    Starts the server. If started in debug mode, the watchdog that kills the server after
    30 seconds without usage is not started. The authorization key is set to "DebugMode".
*/
void RemoteServer::start()
{
    Q_D(RemoteServer);
    if (d->m_tcpServer)
        return;

    d->m_tcpServer = new TcpServer(d->m_port, d->m_address, this);
    d->m_tcpServer->moveToThread(&d->m_thread);
    connect(&d->m_thread, SIGNAL(finished()), d->m_tcpServer, SLOT(deleteLater()));
    connect (d->m_tcpServer, SIGNAL(newIncommingConnection()), this, SLOT(restartWatchdog()));
    d->m_thread.start();

    if (d->m_mode == RemoteServer::Release) {
        connect(d->m_watchdog.data(), SIGNAL(timeout()), this, SLOT(deleteLater()));
        d->m_watchdog->start();
    } else {
        setAuthorizationKey(QLatin1String(Protocol::DebugAuthorizationKey));
    }
}

/*!
    Returns the authorization key.
*/
QString RemoteServer::authorizationKey() const
{
    Q_D(const RemoteServer);
    return d->m_key;
}

/*!
    Sets the authorization key \a authorizationKey this server is asking the clients for.
*/
void RemoteServer::setAuthorizationKey(const QString &authorizationKey)
{
    Q_D(RemoteServer);
    d->m_key = authorizationKey;
}

void RemoteServer::init(quint16 port, const QHostAddress &address, Mode mode)
{
    Q_D(RemoteServer);
    d->m_port = port;
    d->m_address = address;
    d->m_mode = mode;
}

/*!
    Restarts the watchdog that tries to kill the server.
*/
void RemoteServer::restartWatchdog()
{
    Q_D(RemoteServer);
    if (d->m_watchdog)
        d->m_watchdog->start();
}

} // namespace QInstaller