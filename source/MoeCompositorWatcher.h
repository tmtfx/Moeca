// -*- c++ -*-
//
//  'Moeca' window sitter for Haiku.
//

//  Watches whether the Haiku 2D compositor (a "Pirati Del Frico" fork
//  feature, toggled live from the Screen preflet) is currently enabled,
//  by reading the same Screen_data settings file the preflet writes to
//  -- no app_server protocol change needed. Notifies observers whenever
//  the flag changes so they can switch rendering strategy.


#ifndef MOE_COMPOSITOR_WATCHER_H
#define MOE_COMPOSITOR_WATCHER_H


#include <SupportDefs.h>
#include <Handler.h>
#include <List.h>


class MoeCompositorWatcher : public BHandler
{
public:
  static MoeCompositorWatcher *Watcher(void);

  void Run(void);

  void AddObserver(BHandler *handler);
  void RemoveObserver(BHandler *handler);

  bool IsCompositorEnabled(void) const;

protected:
  virtual void MessageReceived(BMessage *msg);

private:
  MoeCompositorWatcher(void);
  virtual ~MoeCompositorWatcher(void);

  void ReadSettings(void);
  void NotifyObservers(void);

  BList mObservers;
  bool mIsCompositorEnabled;
};


#endif
