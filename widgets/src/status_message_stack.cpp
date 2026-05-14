#include "widgets/status_message_stack.h"

#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

void addStatusMessageToLayout(QVBoxLayout* layout, const QString& message, QWidget* parentBlock)
{
    auto* block = new QWidget(parentBlock);
    auto* vl    = new QVBoxLayout(block);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(4);

    auto* msgLine = new QLineEdit(message, block);
    msgLine->setReadOnly(true);
    msgLine->setFrame(false);
    msgLine->setFocusPolicy(Qt::ClickFocus);
    msgLine->setCursorMoveStyle(Qt::LogicalMoveStyle);
    msgLine->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    msgLine->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    msgLine->setStyleSheet(
        QStringLiteral("QLineEdit { background: transparent; color: #6b7280; font-size: 13px;"
                       " padding: 0; margin: 0; border: none;"
                       " selection-background-color: #bfdbfe; selection-color: #111827; }"));
    msgLine->ensurePolished();
    const QFontMetrics fm(msgLine->font());
    msgLine->setFixedHeight(fm.lineSpacing() + 2);
    msgLine->setCursorPosition(0);
    msgLine->deselect();

    auto* moreBtn = new QPushButton(parentBlock->tr("More"), block);
    moreBtn->setFlat(true);
    moreBtn->setCursor(Qt::PointingHandCursor);
    moreBtn->setStyleSheet(
        QStringLiteral("QPushButton { color: #2563eb; font-size: 13px; padding: 0 4px; border: none; }"
                       "QPushButton:hover { text-decoration: underline; }"));

    auto* collapsedRow = new QWidget(block);
    auto* hCollapsed   = new QHBoxLayout(collapsedRow);
    hCollapsed->setContentsMargins(0, 0, 0, 0);
    hCollapsed->setSpacing(8);
    hCollapsed->addWidget(msgLine, 1);
    hCollapsed->addWidget(moreBtn, 0, Qt::AlignRight);

    auto* expandedBlock = new QWidget(block);
    auto* vExp          = new QVBoxLayout(expandedBlock);
    vExp->setContentsMargins(0, 0, 0, 0);
    vExp->setSpacing(4);

    auto* msgFull = new QPlainTextEdit(message, expandedBlock);
    msgFull->setReadOnly(true);
    msgFull->setFrameShape(QFrame::NoFrame);
    msgFull->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    msgFull->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    msgFull->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    msgFull->setTabChangesFocus(false);
    msgFull->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    msgFull->document()->setDocumentMargin(0);
    msgFull->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: transparent; color: #6b7280; font-size: 13px;"
                       " padding: 0; margin: 0; border: none;"
                       " selection-background-color: #bfdbfe; selection-color: #111827; }"));

    auto* expandedFoot = new QWidget(expandedBlock);
    auto* hFoot         = new QHBoxLayout(expandedFoot);
    hFoot->setContentsMargins(0, 0, 0, 0);
    hFoot->setSpacing(0);
    hFoot->addStretch(1);

    vExp->addWidget(msgFull);
    vExp->addWidget(expandedFoot);

    vl->addWidget(collapsedRow);
    vl->addWidget(expandedBlock);
    expandedBlock->hide();

    QObject::connect(moreBtn, &QPushButton::clicked, block,
                     [msgLine, msgFull, moreBtn, parentBlock, collapsedRow, expandedBlock, hCollapsed, hFoot,
                      expandedFoot]() {
                         if (expandedBlock->isVisible())
                         {
                             hFoot->removeWidget(moreBtn);
                             moreBtn->setParent(collapsedRow);
                             hCollapsed->addWidget(moreBtn, 0, Qt::AlignRight);
                             expandedBlock->hide();
                             collapsedRow->show();
                             msgLine->show();
                             msgLine->setCursorPosition(0);
                             msgLine->deselect();
                             moreBtn->setText(parentBlock->tr("More"));
                         }
                         else
                         {
                             hCollapsed->removeWidget(moreBtn);
                             moreBtn->setParent(expandedFoot);
                             hFoot->addWidget(moreBtn, 0, Qt::AlignRight);
                             collapsedRow->hide();
                             expandedBlock->show();
                             moreBtn->setText(parentBlock->tr("Less"));
                         }
                     });

    layout->addWidget(block);
}
