//============================================================================
/// \file   VirtualKeyboardInputContext.cpp
/// \author Uwe Kindler
/// \date   08.01.2015
/// \brief  Implementation of VirtualKeyboardInputContext
///
/// Copyright 2015 Uwe Kindler
/// Licensed under MIT see LICENSE.MIT in project root
//============================================================================

//============================================================================
//                                 INCLUDES
//============================================================================
#include "VirtualKeyboardInputContext.h"

#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQmlContext>
#include <QVariant>
#include <QQmlEngine>
#include <QJSEngine>
#include <QPropertyAnimation>
#include <QTimer>

#include <private/qquickflickable_p.h>
#include "DeclarativeInputEngine.h"


/**
 * Private data class for VirtualKeyboardInputContext
 */
class VirtualKeyboardInputContextPrivate
{
public:
    VirtualKeyboardInputContextPrivate();
    QQuickFlickable* Flickable;
    QQuickItem* FocusItem;
    bool Visible;
    DeclarativeInputEngine* InputEngine;
    QPropertyAnimation* FlickableContentScrollAnimation;//< for smooth scrolling of flickable content item
};


//==============================================================================
VirtualKeyboardInputContextPrivate::VirtualKeyboardInputContextPrivate()
    : Flickable(0),
      FocusItem(0),
      Visible(false),
      InputEngine(new DeclarativeInputEngine())
{

}


//==============================================================================
VirtualKeyboardInputContext::VirtualKeyboardInputContext() :
    QPlatformInputContext(), d(new VirtualKeyboardInputContextPrivate)
{
    d->FlickableContentScrollAnimation = new QPropertyAnimation(this);
    d->FlickableContentScrollAnimation->setPropertyName("contentY");
    d->FlickableContentScrollAnimation->setDuration(400);
    d->FlickableContentScrollAnimation->setEasingCurve(QEasingCurve(QEasingCurve::OutBack));
    qmlRegisterSingletonType<DeclarativeInputEngine>("FreeVirtualKeyboard", 1, 0,
        "InputEngine", inputEngineProvider);
    connect(d->InputEngine, SIGNAL(animatingChanged()), this, SLOT(ensureFocusedObjectVisible()));


}


//==============================================================================
VirtualKeyboardInputContext::~VirtualKeyboardInputContext()
{

}


//==============================================================================
VirtualKeyboardInputContext* VirtualKeyboardInputContext::instance()
{
    static VirtualKeyboardInputContext* InputContextInstance = new VirtualKeyboardInputContext;
    return InputContextInstance;
}



//==============================================================================
bool VirtualKeyboardInputContext::isValid() const
{
    return true;
}


//==============================================================================
QRectF VirtualKeyboardInputContext::keyboardRect() const
{
    return QRectF();
}


//==============================================================================
void VirtualKeyboardInputContext::showInputPanel()
{
    d->Visible = true;
    QPlatformInputContext::showInputPanel();
    emitInputPanelVisibleChanged();
}


//==============================================================================
void VirtualKeyboardInputContext::hideInputPanel()
{
    if (d->FocusItem) {
        d->FocusItem->setFocus(false);
    }
    d->Visible = false;
    QPlatformInputContext::hideInputPanel();
    emitInputPanelVisibleChanged();
}


//==============================================================================
bool VirtualKeyboardInputContext::isInputPanelVisible() const
{
    return d->Visible;
}


//==============================================================================
bool VirtualKeyboardInputContext::isAnimating() const
{
    return false;
}

//==============================================================================
bool VirtualKeyboardInputContext::filterEvent(const QEvent *event)
{
    if (!event || event->type() != QEvent::KeyPress) {
        return false;
    }

    const QKeyEvent *keyEvent = static_cast<const QKeyEvent *>(event);
    if (keyEvent->key() != Qt::Key_Enter && keyEvent->key() != Qt::Key_Return) {
        return false;
    }

    if (!d->Visible || !d->FocusItem || !d->FocusItem->inputMethodQuery(Qt::ImEnabled).toBool()) {
        return false;
    }

    QTimer::singleShot(0, this, [this]() {
        if (d->FocusItem) {
            d->FocusItem->setFocus(false);
        }
        hideInputPanel();
    });

    return false;
}


//==============================================================================
void VirtualKeyboardInputContext::setFocusObject(QObject *object)
{
    static const int NumericInputHints = Qt::ImhPreferNumbers | Qt::ImhDate
        | Qt::ImhTime | Qt::ImhDigitsOnly | Qt::ImhFormattedNumbersOnly;
    static const int DialableInputHints = Qt::ImhDialableCharactersOnly;



    if (!object)
    {
        hideInputPanel();
        return;
    }

    // we only support QML at the moment - so if this is not a QML item, then
    // we leave immediatelly
    d->FocusItem = dynamic_cast<QQuickItem*>(object);
    if (!d->FocusItem)
    {
        hideInputPanel();
        return;
    }

    // Check if an input control has focus that accepts text input - if not,
    // then we can leave immediatelly
    bool AcceptsInput = d->FocusItem->inputMethodQuery(Qt::ImEnabled).toBool();
    if (!AcceptsInput)
    {
        hideInputPanel();


        return;
    }

    // Set input mode depending on input method hints queried from focused
    // object / item
    Qt::InputMethodHints InputMethodHints(d->FocusItem->inputMethodQuery(Qt::ImHints).toInt());
    qDebug() << QString("InputMethodHints: %1").arg(InputMethodHints, 0, 16);
    if (InputMethodHints & DialableInputHints)
    {
        d->InputEngine->setInputMode(DeclarativeInputEngine::Dialable);
    }
    else if (InputMethodHints & NumericInputHints)
    {
        d->InputEngine->setInputMode(DeclarativeInputEngine::Numeric);
    }
    else
    {
        d->InputEngine->setInputMode(DeclarativeInputEngine::Latin);
    }

    // Search for the top most flickable so that we can scroll the control
    // into the visible area, if the keyboard hides the control
    QQuickItem* i = d->FocusItem;
    d->Flickable = nullptr;
    while (i)
    {
        QQuickFlickable* Flickable = dynamic_cast<QQuickFlickable*>(i);
        if (Flickable)
        {
            d->Flickable = Flickable;
            qDebug() << "is QQuickFlickable";
        }
        i = i->parentItem();
    }

    ensureFocusedObjectVisible();
}


//==============================================================================
void VirtualKeyboardInputContext::ensureFocusedObjectVisible()
{
    // If the keyboard is hidden, no scrollable element exists or the keyboard
    // is just animating, then we leave here

    if(!d->Visible && d->Flickable){
        d->FlickableContentScrollAnimation->setEndValue(0);
        d->FlickableContentScrollAnimation->start();
    }
    if (!d->Visible || !d->Flickable || d->InputEngine->isAnimating())
    {
        return;
    }


    QRectF FocusItemRect(0, 0, d->FocusItem->width(), d->FocusItem->height());
    FocusItemRect = d->Flickable->mapRectFromItem(d->FocusItem, FocusItemRect);


    d->FlickableContentScrollAnimation->setTargetObject(d->Flickable);




    int focusContentOffset=(d->Flickable->height()-d->InputEngine->keyboardRectangle().height())-FocusItemRect.bottom();



    if (focusContentOffset <0 )
    {

        d->FlickableContentScrollAnimation->setEndValue(abs(focusContentOffset));
        d->FlickableContentScrollAnimation->start();
    }
//
}


//==============================================================================
QObject* VirtualKeyboardInputContext::inputEngineProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return VirtualKeyboardInputContext::instance()->d->InputEngine;
}

//------------------------------------------------------------------------------
// EOF VirtualKeyboardInpitContext.cpp
