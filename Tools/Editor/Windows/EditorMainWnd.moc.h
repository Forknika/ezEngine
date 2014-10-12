#pragma once

#include <Foundation/Basics.h>
#include <QMainWindow>
#include <Tools/Editor/ui_EditorMainWnd.h>

class ezEditorMainWnd : public QMainWindow, public Ui_EditorMainWnd
{
public:
  Q_OBJECT

public:
  ezEditorMainWnd();
  ~ezEditorMainWnd();

  static ezEditorMainWnd* GetInstance() { return s_pWidget; }

  virtual void closeEvent(QCloseEvent* event);

public slots:

private slots:
  virtual void on_ActionConfigurePlugins_triggered();

private:
  // Window Layout
  void SaveWindowLayout();
  void RestoreWindowLayout();

  // Plugins
  void LoadPlugins();
  void UnloadPlugins();


  static ezEditorMainWnd* s_pWidget;
};


