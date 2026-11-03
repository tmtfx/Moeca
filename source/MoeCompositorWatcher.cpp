// -*- c++ -*-
//
//  'Moeca' window sitter for Haiku.
//


#include <cstdio>
#include <cstring>
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <NodeMonitor.h>
#include <Node.h>
#include <Message.h>
#include <Application.h>
#include <Locker.h>
#include <Autolock.h>
#include "MoeDefs.h"
#include "MoeCompositorWatcher.h"


// Name of the settings file Haiku's Screen preflet writes to, and the
// on-disk layout it uses (ScreenSettings.cpp in the Haiku fork):
// a BPoint window offset, followed by four bools in this order:
// hardwareCursorEnabled, acceleration2DEnabled, compositor2DEnabled,
// visual2DDebugEnabled. The preflet persists it synchronously on every
// toggle, so reading it is an accurate, zero-risk way to learn whether
// the live 2D compositor is on, without talking to app_server at all.
static const char *kScreenSettingsFile = "Screen_data";


static BLocker sLocker;
static MoeCompositorWatcher *sWatcher;


MoeCompositorWatcher *
MoeCompositorWatcher::Watcher(void)
{
  BAutolock lock(sLocker);

  if (! sWatcher)
    sWatcher = new MoeCompositorWatcher();

  return sWatcher;
}


MoeCompositorWatcher::MoeCompositorWatcher(void)
  : BHandler("compositor watcher")
  , mIsCompositorEnabled(false)
{
}


MoeCompositorWatcher::~MoeCompositorWatcher(void)
{
}


void
MoeCompositorWatcher::Run(void)
{
  be_app->AddHandler(this);

  this->ReadSettings();

  BPath path;
  if (::find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
    return;

  path.Append(kScreenSettingsFile);

  BNode node(path.Path());
  node_ref ref;

  if (node.InitCheck() == B_OK &&
      node.GetNodeRef(&ref) == B_OK) {
    status_t watchStatus = ::watch_node(&ref, B_WATCH_STAT, this);
    fprintf(stderr, "MoeCompositorWatcher: watching %s (watch_node: %s)\n",
	    path.Path(), strerror(watchStatus));
  } else {
    fprintf(stderr, "MoeCompositorWatcher: could not watch %s "
	    "(node InitCheck/GetNodeRef failed)\n", path.Path());
  }
}


bool
MoeCompositorWatcher::IsCompositorEnabled(void) const
{
  return mIsCompositorEnabled;
}


void
MoeCompositorWatcher::AddObserver(BHandler *handler)
{
  BAutolock lock(sLocker);

  mObservers.AddItem(handler);
}


void
MoeCompositorWatcher::RemoveObserver(BHandler *handler)
{
  BAutolock lock(sLocker);

  mObservers.RemoveItem(handler);
}


void
MoeCompositorWatcher::NotifyObservers(void)
{
  BAutolock lock(sLocker);
  BMessage msg(MOE_COMPOSITOR_CHANGED);
  int32 i;

  for (i = 0; i < mObservers.CountItems(); i++)
    {
      BHandler *observer = reinterpret_cast<BHandler*>(mObservers.ItemAt(i));
      observer->Looper()->PostMessage(&msg, observer);
    }
}


void
MoeCompositorWatcher::ReadSettings(void)
{
  BPath path;
  if (::find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
    {
      fprintf(stderr, "MoeCompositorWatcher: find_directory failed\n");
      return;
    }

  path.Append(kScreenSettingsFile);

  BFile file(path.Path(), B_READ_ONLY);
  status_t initCheck = file.InitCheck();
  if (initCheck != B_OK)
    {
      fprintf(stderr, "MoeCompositorWatcher: cannot open %s (%s)\n",
	      path.Path(), strerror(initCheck));
      return;
    }

  BPoint offset;
  bool hardwareCursor, acceleration2D, compositor2D;

  if (file.Read(&offset, sizeof(offset)) != (ssize_t)sizeof(offset) ||
      file.Read(&hardwareCursor, sizeof(bool)) != (ssize_t)sizeof(bool) ||
      file.Read(&acceleration2D, sizeof(bool)) != (ssize_t)sizeof(bool) ||
      file.Read(&compositor2D, sizeof(bool)) != (ssize_t)sizeof(bool))
    {
      fprintf(stderr, "MoeCompositorWatcher: %s is too short/unexpected "
	      "layout, can't read the compositor flag\n", path.Path());
      return;
    }

  fprintf(stderr, "MoeCompositorWatcher: read %s -> hardwareCursor=%d "
	  "acceleration2D=%d compositor2D=%d (previously cached: %d)\n",
	  path.Path(), hardwareCursor, acceleration2D, compositor2D,
	  mIsCompositorEnabled);

  BAutolock lock(sLocker);

  if (mIsCompositorEnabled == compositor2D)
    return;

  mIsCompositorEnabled = compositor2D;
  this->NotifyObservers();
}


void
MoeCompositorWatcher::MessageReceived(BMessage *msg)
{
  switch (msg->what)
    {
    default:
      BHandler::MessageReceived(msg);
      break;

    case B_NODE_MONITOR:
      this->ReadSettings();
      break;
    }
}
