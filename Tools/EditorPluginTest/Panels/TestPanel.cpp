#include <PCH.h>
#include <EditorPluginTest/Panels/TestPanel.moc.h>
#include <QSettings>

ezTestPanel::ezTestPanel(QWidget* parent) : QDockWidget(parent)
{
  setupUi(this);
}

ezTestPanel::~ezTestPanel()
{
}
