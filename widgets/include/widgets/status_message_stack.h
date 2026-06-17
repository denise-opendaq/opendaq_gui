#pragma once

#include <QString>
#include <QVBoxLayout>

class QWidget;

/** Single-line read-only field; "More" / "Less" toggles full plain-text view (same text). */
void addStatusMessageToLayout(QVBoxLayout* layout, const QString& message, QWidget* parentBlock);
