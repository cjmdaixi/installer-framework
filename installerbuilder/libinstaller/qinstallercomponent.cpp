/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "qinstallercomponent.h"

#include "common/errors.h"
#include "common/fileutils.h"
#include "common/utils.h"
#include "fsengineclient.h"
#include "lib7z_facade.h"
#include "qinstaller.h"
#include "qinstallercomponent_p.h"
#include "qinstallerglobal.h"
#include "messageboxhandler.h"

#include <KDUpdater/Update>
#include <KDUpdater/UpdateSourcesInfo>
#include <KDUpdater/UpdateOperation>
#include <KDUpdater/UpdateOperationFactory>
#include <KDUpdater/PackagesInfo>

#include <QtCore/QDirIterator>
#include <QtCore/QTranslator>

#include <QtGui/QApplication>

#include <QtUiTools/QUiLoader>

#include <algorithm>

using namespace QInstaller;

static const QLatin1String skName("Name");
static const QLatin1String skDisplayName("DisplayName");
static const QLatin1String skDescription("Description");
static const QLatin1String skCompressedSize("CompressedSize");
static const QLatin1String skUncompressedSize("UncompressedSize");
static const QLatin1String skVersion("Version");
static const QLatin1String skDependencies("Dependencies");
static const QLatin1String skReleaseDate("ReleaseDate");
static const QLatin1String skReplaces("Replaces");
static const QLatin1String skVirtual("Virtual");
static const QLatin1String skSortingPriority("SortingPriority");
static const QLatin1String skInstallPriority("InstallPriority");
static const QLatin1String skAutoSelectOn("AutoSelectOn");
static const QLatin1String skImportant("Important");
static const QLatin1String skForcedInstallation("ForcedInstallation");
static const QLatin1String skUpdateText("UpdateText");
static const QLatin1String skRequiresAdminRights("RequiresAdminRights");
static const QLatin1String skNewComponent("NewComponent");
static const QLatin1String skScript("Script");
static const QLatin1String skInstalledVersion("InstalledVersion");

/*
    TRANSLATOR QInstaller::Component
*/

/*!
    \class QInstaller::Component
    Component describes a component within the installer.
*/

/*!
    Constructor. Creates a new Component inside of \a installer.
*/
Component::Component(Installer *installer)
    : d(new ComponentPrivate(installer, this))
{
    d->init();
    setPrivate(d);

    connect(this, SIGNAL(valueChanged(QString, QString)), this, SLOT(updateModelData(QString, QString)));
}

Component::Component(KDUpdater::Update* update, Installer* installer)
    : d(new ComponentPrivate(installer, this))
{
    Q_ASSERT(update);

    d->init();
    setPrivate(d);
    connect(this, SIGNAL(valueChanged(QString, QString)), this, SLOT(updateModelData(QString, QString)));

    loadDataFromUpdate(update);
}

/*!
    Destroys the Component.
*/
Component::~Component()
{
    if (parentComponent() != 0)
        d->m_parent->d->m_allComponents.removeAll(this);

    if (!d->m_newlyInstalled)
        qDeleteAll(d->operations);

    qDeleteAll(d->m_allComponents);
    delete d;
}

//package info is that what is saved inside the packagemanager on harddisk
void Component::loadDataFromPackageInfo(const KDUpdater::PackageInfo &packageInfo)
{
    setValue(skName, packageInfo.name);
    setValue(skDisplayName, packageInfo.title);
    setValue(skDescription, packageInfo.description);
    setValue(skUncompressedSize, QString::number(packageInfo.uncompressedSize));
    setValue(skVersion, packageInfo.version);
    setValue(skVirtual, packageInfo.virtualComp ? QLatin1String ("true") : QLatin1String ("false"));

    QString dependstr = QLatin1String("");
    foreach (const QString& val, packageInfo.dependencies)
        dependstr += val + QLatin1String(",");

    if (packageInfo.dependencies.count() > 0)
        dependstr.chop(1);
    setValue(skDependencies, dependstr);

    setValue(skForcedInstallation, packageInfo.forcedInstallation ? QLatin1String ("true")
        : QLatin1String ("false"));
    if (packageInfo.forcedInstallation) {
        setEnabled(false);
        setCheckable(false);
        setCheckState(Qt::Checked);
    }
}

//update means it is the packageinfo from server
void Component::loadDataFromUpdate(KDUpdater::Update* update)
{
    Q_ASSERT(update);
    Q_ASSERT(!update->name().isEmpty());

    setValue(skName, update->data(skName).toString());
    setValue(skDisplayName, update->data(skDisplayName).toString());
    setValue(skDescription, update->data(skDescription).toString());
    setValue(skCompressedSize, QString::number(update->compressedSize()));
    setValue(skUncompressedSize, QString::number(update->uncompressedSize()));
    setValue(skVersion, update->data(skVersion).toString());
    setValue(skDependencies, update->data(skDependencies).toString());
    setValue(skVirtual, update->data(skVirtual).toString());
    setValue(skSortingPriority, update->data(skSortingPriority).toString());
    setValue(skInstallPriority, update->data(skInstallPriority).toString());
    setValue(skAutoSelectOn, update->data(skAutoSelectOn).toString());

    setValue(skImportant, update->data(skImportant).toString());
    setValue(skUpdateText, update->data(skUpdateText).toString());
    setValue(skNewComponent, update->data(skNewComponent).toString());
    setValue(skRequiresAdminRights, update->data(skRequiresAdminRights).toString());

    setValue(skScript, update->data(skScript).toString());
    setValue(skReplaces, update->data(skReplaces).toString());
    setValue(skReleaseDate, update->data(skReleaseDate).toString());

    QString forced = update->data(skForcedInstallation).toString().toLower();
    if (qApp->arguments().contains(QLatin1String("--no-force-installations")))
        forced = QLatin1String("false");
    setValue(skForcedInstallation, forced);
    if (forced == QLatin1String("true")) {
        setEnabled(false);
        setCheckable(false);
        setCheckState(Qt::Checked);
    }

    setLocalTempPath(QInstaller::pathFromUrl(update->sourceInfo().url));
    const QStringList uis = update->data(QLatin1String("UserInterfaces")).toString()
        .split(QString::fromLatin1(","), QString::SkipEmptyParts);
    if (!uis.isEmpty())
        loadUserInterfaces(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), uis);

    const QStringList qms = update->data(QLatin1String("Translations")).toString()
        .split(QString::fromLatin1(","), QString::SkipEmptyParts);
    if (!qms.isEmpty())
        loadTranslations(QDir(QString::fromLatin1("%1/%2").arg(localTempPath(), name())), qms);

    QHash<QString, QVariant> licenseHash = update->data(QLatin1String("Licenses")).toHash();
    if (!licenseHash.isEmpty())
        loadLicenses(QString::fromLatin1("%1/%2/").arg(localTempPath(), name()), licenseHash);
}

QString Component::uncompressedSize() const
{
    double size = value(skUncompressedSize).toDouble();
    if (size < 10000.0)
        return tr("%L1 Bytes").arg(size);
    size /= 1024.0;
    if (size < 10000.0)
        return tr("%L1 kBytes").arg(size, 0, 'f', 1);
    size /= 1024.0;
    if (size < 10000.0)
        return tr("%L1 MBytes").arg(size, 0, 'f', 1);
    size /= 1024.0;
    return tr("%L1 GBytes").arg(size, 0, 'f', 1);
}

void Component::markAsPerformedInstallation()
{
    d->m_newlyInstalled = true;
}

/*!
    \property Component::removeBeforeUpdate
    Specifies wheter this component gets removed by the installer system before it gets updated.
    Get this property's value by using %removeBeforeUpdate(), and set it
    using %setRemoveBeforeUpdate(). The default value is true.
*/
bool Component::removeBeforeUpdate() const
{
    return d->removeBeforeUpdate;
}

void Component::setRemoveBeforeUpdate(bool removeBeforeUpdate)
{
    d->removeBeforeUpdate = removeBeforeUpdate;
}

QList<Component*> Component::dependees() const
{
    return d->m_installer->dependees(this);
}

/*
    Returns a key/value based hash of all variables set for this component.
*/
QHash<QString,QString> Component::variables() const
{
    return d->m_vars;
}

/*!
    Returns the value of variable name \a key.
    If \a key is not known yet, \a defaultValue is returned.
*/
QString Component::value(const QString &key, const QString &defaultValue) const
{
    return d->m_vars.value(key, defaultValue);
}

/*!
    Sets the value of the variable with \a key to \a value.
*/
void Component::setValue(const QString &key, const QString &value)
{
    if (d->m_vars.value(key) == value)
        return;

    d->m_vars[key] = value;
    emit valueChanged(key, value);
}

/*!
    Returnst the installer this component belongs to.
*/
Installer* Component::installer() const
{
    return d->m_installer;
}

/*!
    Returns the parent of this component. If this component is com.nokia.sdk.qt, its
    parent is com.nokia.sdk, as far as this exists.
*/
Component* Component::parentComponent(RunMode runMode) const
{
    if (runMode == UpdaterMode)
        return 0;
    return d->m_parent;
}

/*!
    Appends \a component as a child of this component. If \a component already has a parent,
    it is removed from the previous parent.
*/
void Component::appendComponent(Component* component)
{
    if (component->value(skVirtual).toLower() != QLatin1String("true")) {
        d->m_components.append(component);
        std::sort(d->m_components.begin(), d->m_components.end(), Component::SortingPriorityLessThan());
    } else {
        d->m_virtualComponents.append(component);
    }

    d->m_allComponents = d->m_components + d->m_virtualComponents;
    if (Component *parent = component->parentComponent())
        parent->removeComponent(component);
    component->d->m_parent = this;
    setTristate(childCount() > 0);
}

/*!
    Removes \a component if it is a child of this component. The component object still exists after the
    function returns. It's up to the caller to delete the passed component.
*/
void Component::removeComponent(Component *component)
{
    if (component->parentComponent() == this) {
        component->d->m_parent = 0;
        d->m_components.removeAll(component);
        d->m_virtualComponents.removeAll(component);
        d->m_allComponents = d->m_components + d->m_virtualComponents;
    }
}

/*!
    Returns a list of child components. If \a recursive is set to true, the returned list
    contains not only the direct children, but all ancestors.
*/
QList<Component*> Component::childComponents(bool recursive, RunMode runMode) const
{
    if (runMode == UpdaterMode)
        return QList<Component*>();

    if (!recursive)
        return d->m_allComponents;

    QList<Component*> result;
    foreach (Component *component, d->m_allComponents) {
        result.append(component);
        result += component->childComponents(true);
    }
    return result;
}

/*!
    Contains this component's name (unique identifier).
*/
QString Component::name() const
{
    return value(skName);
}

/*!
    Contains this component's display name (as visible to the user).
*/
QString Component::displayName() const
{
    return value(skDisplayName);
}

void Component::loadComponentScript()
{
    const QString script = value(skScript);
    if (!localTempPath().isEmpty() && !script.isEmpty())
        loadComponentScript(QString::fromLatin1("%1/%2/%3").arg(localTempPath(), name(), script));
}

/*!
    Loads the script at \a fileName into this component's script engine. The installer and all its
    components as well as other useful stuff are being exported into the script.
    Read \link componentscripting Component Scripting \endlink for details.
    \throws Error when either the script at \a fileName couldn't be opened, or the QScriptEngine
    couldn't evaluate the script.
*/
void Component::loadComponentScript(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        throw Error(QObject::tr("Could not open the requested script file at %1: %2").arg(fileName,
            file.errorString()));
    }

    d->scriptEngine.evaluate(QLatin1String(file.readAll()), fileName);
    if (d->scriptEngine.hasUncaughtException()) {
        throw Error(QObject::tr("Exception while loading the component script %1")
            .arg(uncaughtExceptionString(&(d->scriptEngine)/*, QFileInfo(file).absoluteFilePath()*/)));
    }

    const QList<Component*> components = d->m_installer->components(true, d->m_installer->runMode());
    QScriptValue comps = d->scriptEngine.newArray(components.count());
    for (int i = 0; i < components.count(); ++i)
        comps.setProperty(i, d->scriptEngine.newQObject(components[i]));

    d->scriptEngine.globalObject().property(QLatin1String("installer"))
        .setProperty(QLatin1String("components"), comps);

    QScriptValue comp = d->scriptEngine.evaluate(QLatin1String("Component"));
    if (!d->scriptEngine.hasUncaughtException()) {
        d->scriptComponent = comp;
        d->scriptComponent.construct();
    }

    emit loaded();
    languageChanged();
}

/*!
    \internal
    Calls the script method \link retranslateUi() \endlink, if any. This is done whenever a
    QTranslator file is being loaded.
*/
void Component::languageChanged()
{
    callScriptMethod(QLatin1String("retranslateUi"));
}

/*!
    Tries to call the method with \a name within the script and returns the result. If the method
    doesn't exist, an invalid result is returned. If the method has an uncaught exception, its
    string representation is thrown as an Error exception.

    \note The method is not called, if the current script context is the same method, to avoid
    infinite recursion.
*/
QScriptValue Component::callScriptMethod(const QString &methodName, const QScriptValueList& arguments)
{
    if (!d->unexistingScriptMethods.value(methodName, true))
        return QScriptValue();

    // don't allow such a recursion
    if (d->scriptEngine.currentContext()->backtrace().first().startsWith(methodName))
        return QScriptValue();

    QScriptValue method = d->scriptComponent.property(QString::fromLatin1("prototype"))
        .property(methodName);
    if (!method.isValid()) // this marks the method to be called not any longer
        d->unexistingScriptMethods[methodName] = false;

    const QScriptValue result = method.call(d->scriptComponent, arguments);
    if (!result.isValid())
        return result;

    if (d->scriptEngine.hasUncaughtException())
        throw Error(uncaughtExceptionString(&(d->scriptEngine)/*, name()*/));

    return result;
}

/*!
    Loads the translations matching the name filters \a qms inside \a directory. Only translations
    with a \link QFileInfo::baseName() baseName \endlink matching the current locales \link
    QLocale::name() name \endlink are loaded.
    Read \ref componenttranslation for details.
*/
void Component::loadTranslations(const QDir& directory, const QStringList& qms)
{
    QDirIterator it(directory.path(), qms, QDir::Files);
    while (it.hasNext()) {
        const QString filename = it.next();
        if (QFileInfo(filename).baseName().toLower() != QLocale().name().toLower())
            continue;

        QScopedPointer<QTranslator> translator(new QTranslator(this));
        if (!translator->load(filename))
            throw Error(tr("Could not open the requested translation file at %1").arg(filename));
        qApp->installTranslator(translator.take());
    }
}

/*!
    Loads the user interface files matching the name filters \a uis inside \a directory. The loaded
    interface can be accessed via userInterfaces by using the class name set in the ui file.
    Read \ref componentuserinterfaces for details.
*/
void Component::loadUserInterfaces(const QDir& directory, const QStringList& uis)
{
    if (QApplication::type() == QApplication::Tty)
        return;

    QDirIterator it(directory.path(), uis, QDir::Files);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open the requested UI file at %1: %2").arg(it.fileName(),
                file.errorString()));
        }

        static QUiLoader loader;
        loader.setTranslationEnabled(true);
        loader.setLanguageChangeEnabled(true);
        QWidget* const w = loader.load(&file);
        d->userInterfaces[w->objectName()] = w;
    }
}


void Component::loadLicenses(const QString &directory, const QHash<QString, QVariant> &licenseHash)
{
    QHash<QString, QVariant>::const_iterator it;
    for (it = licenseHash.begin(); it != licenseHash.end(); ++it) {
        const QString &fileName = it.value().toString();
        QFile file(directory + fileName);
        if (!file.open(QIODevice::ReadOnly)) {
            throw Error(tr("Could not open the requested license file at %1: %2").arg(fileName,
                file.errorString()));
        }
        d->m_licenses.insert(it.key(), qMakePair(fileName, QTextStream(&file).readAll()));
    }
}

/*!
    Contains a list of all user interface class names known to this component.
*/
QStringList Component::userInterfaces() const
{
    return d->userInterfaces.keys();
}

QHash<QString, QPair<QString, QString> > Component::licenses() const
{
    return d->m_licenses;
}

/*!
    Returns the QWidget created for class \a name.
*/
QWidget* Component::userInterface(const QString &name) const
{
    return d->userInterfaces.value(name);
}

/*!
    Creates all operations needed to install this component's \a path. \a path is a full qualified
    filename including the component's name. This metods gets called from
    Component::createOperationsForArchive. You can override this method by providing a method with
    the same name in the component script.

    \note RSA signature files are omitted by this method.
    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implemention is recursively creating Copy and Mkdir operations for all files
    and folders within \a path.
*/
void Component::createOperationsForPath(const QString &path)
{
    const QFileInfo fi(path);

    // don't copy over a signature
    if (fi.suffix() == QLatin1String("sig") && QFileInfo(fi.dir(), fi.completeBaseName()).exists())
        return;

    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperationsForPath"), QScriptValueList() << path).isValid())
        return;

    QString target;
    static const QString zipPrefix = QString::fromLatin1("7z://installer://");
    // if the path is an archive, remove the archive file name from the target path
    if (path.startsWith(zipPrefix)) {
        target = path.mid(zipPrefix.length() + name().length() + 1); // + 1 for the /
        const int nextSlash = target.indexOf(QLatin1Char('/'));
        if (nextSlash != -1)
            target = target.mid(nextSlash);
        else
            target.clear();
        target.prepend(QLatin1String("@TargetDir@"));
    } else {
        static const QString prefix = QString::fromLatin1("installer://");
        target = QString::fromLatin1("@TargetDir@%1").arg(path.mid(prefix.length() + name().length()));
    }

    if (fi.isFile()) {
        static const QString copy = QString::fromLatin1("Copy");
        addOperation(copy, fi.filePath(), target);
    } else if (fi.isDir()) {
        qApp->processEvents();
        static const QString mkdir = QString::fromLatin1("Mkdir");
        addOperation(mkdir, target);

        QDirIterator it(fi.filePath());
        while (it.hasNext())
            createOperationsForPath(it.next());
    }
}

/*!
    Creates all operations needed to install this component's \a archive. This metods gets called
    from Component::createOperations. You can override this method by providing a method with the
    same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForPath for everything contained in the archive.
    If \a archive is a compressed archive known to the installer system, an Extract operation is
    created, instead.
*/
void Component::createOperationsForArchive(const QString &archive)
{
    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperationsForArchive"), QScriptValueList() << archive).isValid())
        return;

    const QFileInfo fi(QString::fromLatin1("installer://%1/%2").arg(name(), archive));
    const bool isZip = Lib7z::isSupportedArchive(fi.filePath());

    if (isZip) {
        // archives get completely extracted per default (if the script isn't doing other stuff)
        addOperation(QLatin1String("Extract"), fi.filePath(), QLatin1String("@TargetDir@"));
    } else {
        createOperationsForPath(fi.filePath());
    }
}

/*!
    Creates all operations needed to install this component.
    You can override this method by providing a method with the same name in the component script.

    \note If you call this method from a script, it won't call the scripts method with the same name.

    The default implementation calls createOperationsForArchive for all archives in this component.
*/
void Component::createOperations()
{
    // the script can override this method
    if (callScriptMethod(QLatin1String("createOperations")).isValid()) {
        d->operationsCreated = true;
        return;
    }

    foreach (const QString &archive, archives())
        createOperationsForArchive(archive);

    d->operationsCreated = true;
}

/*!
    Registers the file or directory at \a path for being removed when this component gets uninstalled.
    In case of a directory, this will be recursive. If \a wipe is set to true, the directory will
    also be deleted if it contains changes done by the user after installation.
*/
void Component::registerPathForUninstallation(const QString &path, bool wipe)
{
    d->pathesForUninstallation.append(qMakePair(path, wipe));
}

/*!
    Returns the list of pathes previously registered for uninstallation with
    #registerPathForUninstallation.
*/
QList<QPair<QString, bool> > Component::pathesForUninstallation() const
{
    return d->pathesForUninstallation;
}

/*!
    Contains the names of all archives known to this component. This does not contain archives added
    with #addDownloadableArchive.
*/
QStringList Component::archives() const
{
    return QDir(QString::fromLatin1("installer://%1/").arg(name())).entryList();
}

/*!
    Adds the archive \a path to this component. This can only be called when this component was
    downloaded from an online repository. When adding \a path, it will be downloaded from the
    repository when the installation starts.

    Read \ref sec_repogen for details. \sa fromOnlineRepository
*/
void Component::addDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());

    const QString versionPrefix = value(skVersion);
    verbose() << "addDownloadable " << path << std::endl;
    d->downloadableArchives.append(versionPrefix + path);
}

/*!
    Removes the archive \a path previously added via addDownloadableArchive from this component.
    This can oly be called when this component was downloaded from an online repository.

    Read \ref sec_repogen for details.
*/
void Component::removeDownloadableArchive(const QString &path)
{
    Q_ASSERT(isFromOnlineRepository());
    d->downloadableArchives.removeAll(path);
}

/*!
    Returns the archives to be downloaded from the online repository before installation.
*/
QStringList Component::downloadableArchives() const
{
    return d->downloadableArchives;
}

/*!
    Adds a request for quitting the process @p process before installing/updating/uninstalling the
    component.
*/
void Component::addStopProcessForUpdateRequest(const QString &process)
{
    d->stopProcessForUpdateRequests.append(process);
}

/*!
    Removes the request for quitting the process @p process again.
*/
void Component::removeStopProcessForUpdateRequest(const QString &process)
{
    d->stopProcessForUpdateRequests.removeAll(process);
}

/*!
    Convenience: Add/remove request depending on @p requested (add if @p true, remove if @p false).
*/
void Component::setStopProcessForUpdateRequest(const QString &process, bool requested)
{
    if (requested)
        addStopProcessForUpdateRequest(process);
    else
        removeStopProcessForUpdateRequest(process);
}

/*!
    The list of processes this component needs to be closed before installing/updating/uninstalling
*/
QStringList Component::stopProcessForUpdateRequests() const
{
    return d->stopProcessForUpdateRequests;
}

/*!
    Returns the operations needed to install this component. If autoCreateOperations is true,
    createOperations is called, if no operations have been auto-created yet.
*/
QList<KDUpdater::UpdateOperation*> Component::operations() const
{
    if (d->autoCreateOperations && !d->operationsCreated) {
        const_cast<Component*>(this)->createOperations();

        if (!d->minimumProgressOperation) {
            d->minimumProgressOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("MinimumProgress"));
            d->operations.append(d->minimumProgressOperation);
        }

        if (!d->m_licenses.isEmpty()) {
            d->m_licenseOperation = KDUpdater::UpdateOperationFactory::instance()
                .create(QLatin1String("License"));
            d->m_licenseOperation->setValue(QLatin1String("installer"),
                QVariant::fromValue(d->m_installer));

            QVariantMap licenses;
            const QList<QPair<QString, QString> > values = d->m_licenses.values();
            for (int i = 0; i < values.count(); ++i)
                licenses.insert(values.at(i).first, values.at(i).second);
            d->m_licenseOperation->setValue(QLatin1String("licenses"), licenses);
            d->operations.append(d->m_licenseOperation);
        }
    }
    return d->operations;
}

/*!
    Adds \a operation to the list of operations needed to install this component.
*/
void Component::addOperation(KDUpdater::UpdateOperation* operation)
{
    d->operations.append(operation);
    if (FSEngineClientHandler::instance()->isActive())
        operation->setValue(QLatin1String("admin"), true);
}

/*!
    Adds \a operation to the list of operations needed to install this component. \a operation
    is executed with elevated rights.
*/
void Component::addElevatedOperation(KDUpdater::UpdateOperation* operation)
{
    addOperation(operation);
    operation->setValue(QLatin1String("admin"), true);
}

bool Component::operationsCreatedSuccessfully() const
{
    return d->operationsCreatedSuccessfully;
}

KDUpdater::UpdateOperation* Component::createOperation(const QString &operation,
    const QString &parameter1, const QString &parameter2, const QString &parameter3,
    const QString &parameter4, const QString &parameter5, const QString &parameter6,
    const QString &parameter7, const QString &parameter8, const QString &parameter9,
    const QString &parameter10)
{
    KDUpdater::UpdateOperation* op = KDUpdater::UpdateOperationFactory::instance().create(operation);
    if (op == 0) {
        const QMessageBox::StandardButton button =
            MessageBoxHandler::critical(MessageBoxHandler::currentBestSuitParent(),
            QLatin1String("OperationDoesNotExistError"), tr("Error"),
            tr("Error: Operation %1 does not exist").arg(operation),
            QMessageBox::Abort | QMessageBox::Ignore);
        if (button == QMessageBox::Abort)
            d->operationsCreatedSuccessfully = false;
        return op;
    }

    if (op->name() == QLatin1String("Delete"))
        op->setValue(QLatin1String("performUndo"), false);
    op->setValue(QLatin1String("installer"), qVariantFromValue(d->m_installer));

    QStringList arguments;
    if (!parameter1.isNull())
        arguments.append(parameter1);
    if (!parameter2.isNull())
        arguments.append(parameter2);
    if (!parameter3.isNull())
        arguments.append(parameter3);
    if (!parameter4.isNull())
        arguments.append(parameter4);
    if (!parameter5.isNull())
        arguments.append(parameter5);
    if (!parameter6.isNull())
        arguments.append(parameter6);
    if (!parameter7.isNull())
        arguments.append(parameter7);
    if (!parameter8.isNull())
        arguments.append(parameter8);
    if (!parameter9.isNull())
        arguments.append(parameter9);
    if (!parameter10.isNull())
        arguments.append(parameter10);
    op->setArguments(d->m_installer->replaceVariables(arguments));

    return op;
}
/*!
    Creates and adds an installation operation for \a operation. Add any number of \a parameter1,
    \a parameter2, \a parameter3, \a parameter4, \a parameter5 and \a parameter6. The contents of
    the parameters get variables like "@TargetDir@" replaced with their values, if contained.
    \sa installeroperations
*/
bool Component::addOperation(const QString &operation, const QString &parameter1,
    const QString &parameter2, const QString &parameter3, const QString &parameter4,
    const QString &parameter5, const QString &parameter6, const QString &parameter7,
    const QString &parameter8, const QString &parameter9, const QString &parameter10)
{
    if (KDUpdater::UpdateOperation *op = createOperation(operation, parameter1, parameter2,
        parameter3, parameter4, parameter5, parameter6, parameter7, parameter8, parameter9,
        parameter10)) {
            addOperation(op);
            return true;
    }

    return false;
}

/*!
    Creates and adds an installation operation for \a operation. Add any number of \a parameter1,
    \a parameter2, \a parameter3, \a parameter4, \a parameter5 and \a parameter6. The contents of
    the parameters get variables like "@TargetDir@" replaced with their values, if contained.
    \a operation is executed with elevated rights.
    \sa installeroperations
*/
bool Component::addElevatedOperation(const QString &operation, const QString &parameter1, const QString &parameter2, const QString &parameter3, const QString &parameter4, const QString &parameter5, const QString &parameter6, const QString &parameter7, const QString &parameter8, const QString &parameter9, const QString &parameter10)
{
    if (KDUpdater::UpdateOperation *op = createOperation(operation, parameter1, parameter2,
        parameter3, parameter4, parameter5, parameter6, parameter7, parameter8, parameter9,
        parameter10)) {
            addElevatedOperation(op);
            return true;
    }

    return false;
}

/*!
    Specifies wheter operations should be automatically created when the installation starts. This
    would be done by calling #createOperations. If you set this to false, it's completely up to the
    component's script to create all operations.
*/
bool Component::autoCreateOperations() const
{
    return d->autoCreateOperations;
}

void Component::setAutoCreateOperations(bool autoCreateOperations)
{
    d->autoCreateOperations = autoCreateOperations;
}

/*!
    \property Component::selected
    Specifies wheter this component is selected for installation. Get this property's value by using
    %isSelected(), and set it using %setSelected().
*/
bool Component::isSelected() const
{
    return checkState() != Qt::Unchecked;
}

/*!
    Marks the component for installation. Emits the selectedChanged() signal if the check state changes.
*/
void Component::setSelected(bool selected)
{
    const Qt::CheckState previousState = checkState();
    const Qt::CheckState newState = selected ? Qt::Checked : Qt::Unchecked;

    if (newState != previousState) {
        setCheckState(newState);
        QMetaObject::invokeMethod(this, "selectedChanged", Qt::QueuedConnection,
            Q_ARG(bool, newState == Qt::Checked));
    }
}

/*!
    Contains this component dependencies.
    Read \ref componentdependencies for details.
*/
QStringList Component::dependencies() const
{
    return value(skDependencies).split(QLatin1Char(','));
}

/*!
    Determines if the component is installed
*/
bool Component::isInstalled() const
{
    return QLatin1String("Installed") == value(QLatin1String("CurrentState"));
}

/*!
    Determines if the user wants to install the component
*/
bool Component::installationRequested() const
{
    return !isInstalled() && isSelected();
}

/*!
    Determines if the user wants to install the component
*/
bool Component::uninstallationRequested() const
{
    return isInstalled() && !isSelected();
}

/*!
    Determines if the component was installed recently
*/
bool Component::wasInstalled() const
{
    return QLatin1String("Uninstalled") == value(QLatin1String("PreviousState")) && isInstalled();
}

/*!
    Determines if the component was removed recently
*/
bool Component::wasUninstalled() const
{
    return QLatin1String("Installed") == value(QLatin1String("PreviousState")) && !isInstalled();
}

/*!
    \property Component::fromOnlineRepository

    Determines wheter this component has been loaded from an online repository. Get this property's
    value by usinng %isFromOnlineRepository. \sa addDownloadableArchive
*/
bool Component::isFromOnlineRepository() const
{
    return !repositoryUrl().isEmpty();
}

/*!
    Contains the repository Url this component is downloaded from.
    When this component is not downloaded from an online repository, returns an empty #QUrl.
*/
QUrl Component::repositoryUrl() const
{
    return d->repositoryUrl;
}

/*!
    Sets this components #repositoryUrl.
*/
void Component::setRepositoryUrl(const QUrl& url)
{
    d->repositoryUrl = url;
}


QString Component::localTempPath() const
{
    return d->localTempPath;
}

void Component::setLocalTempPath(const QString &tempLocalPath)
{
    d->localTempPath = tempLocalPath;
}

void Component::updateModelData(const QString &key, const QString &data)
{
    if (key == skVirtual) {
        if (data.toLower() == QLatin1String("true"))
            setData(installer()->virtualComponentsFont(), Qt::FontRole);
    }

    if (key == skVersion)
        setData(data, NewVersion);

    if (key == skDisplayName)
        setData(data, Qt::DisplayRole);

    if (key == skInstalledVersion)
        setData(data, InstalledVersion);

    if (key == skUncompressedSize)
        setData(uncompressedSize(), UncompressedSize);

    setData(value(skDescription) + QLatin1String("<br><br>Update Info: ") + value(skUpdateText),
        Qt::ToolTipRole);
}
