#include "dialogs/load_configuration_dialog.h"
#include "widgets/property_object_view.h"
#include "context/gui_constants.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <opendaq/update_parameters_factory.h>

LoadConfigurationDialog::LoadConfigurationDialog(QWidget* parent)
    : QDialog(parent)
    , updateParameters(daq::UpdateParameters())
    , filePathLabel(nullptr)
    , browseButton(nullptr)
    , loadButton(nullptr)
    , propertyView(nullptr)
{
    setWindowTitle("Load Configuration");
    resize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_HEIGHT);
    setMinimumSize(GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_WIDTH, GUIConstants::ADD_DEVICE_CONFIG_DIALOG_MIN_HEIGHT);

    setupUI();
}

void LoadConfigurationDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // File selection section
    auto* fileGroupBox = new QGroupBox("Configuration File", this);
    auto* fileLayout = new QHBoxLayout(fileGroupBox);

    filePathLabel = new QLabel("No file selected", this);
    filePathLabel->setWordWrap(true);
    filePathLabel->setStyleSheet("QLabel { padding: 5px; background-color: palette(base); border: 1px solid palette(mid); border-radius: 3px; }");
    fileLayout->addWidget(filePathLabel, 1);

    browseButton = new QPushButton("Browse...", this);
    connect(browseButton, &QPushButton::clicked, this, &LoadConfigurationDialog::onBrowseClicked);
    fileLayout->addWidget(browseButton);

    mainLayout->addWidget(fileGroupBox);

    // Update parameters configuration section
    auto* configGroupBox = new QGroupBox("Update Parameters", this);
    auto* configLayout = new QVBoxLayout(configGroupBox);

    propertyView = new PropertyObjectView(updateParameters, this);
    configLayout->addWidget(propertyView);

    mainLayout->addWidget(configGroupBox);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    loadButton = new QPushButton("Load", this);
    loadButton->setDefault(true);
    loadButton->setEnabled(false);
    connect(loadButton, &QPushButton::clicked, this, &LoadConfigurationDialog::onLoadClicked);
    buttonLayout->addWidget(loadButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void LoadConfigurationDialog::onBrowseClicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Select Configuration File",
        "",
        "JSON Files (*.json);;All Files (*)"
    );

    if (!fileName.isEmpty())
    {
        configFilePath = fileName;
        filePathLabel->setText(fileName);
        updateLoadButton();
    }
}

void LoadConfigurationDialog::onLoadClicked()
{
    if (configFilePath.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please select a configuration file.");
        return;
    }

    accept();
}

void LoadConfigurationDialog::updateLoadButton()
{
    loadButton->setEnabled(!configFilePath.isEmpty());
}
