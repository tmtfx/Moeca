// -*- c++ -*-
//
//  'Moe' window sitter for BeOS.
//  AI API Client
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//

#ifndef MOE_AI_CLIENT_H
#define MOE_AI_CLIENT_H

#include <Looper.h>
#include <Messenger.h>
#include <String.h>

class AIEngine;

class MoeAIClient : public BLooper
{
public:
  static MoeAIClient* Client(void);

  void    SendChat(const char* userText, BMessenger replyTo);
  void    ClearHistory(void);
  void    ReloadSettings(void);
  bool    HasApiKey(void) const;

protected:
  virtual void MessageReceived(BMessage* msg);

private:
  MoeAIClient(void);
  virtual ~MoeAIClient(void);

  void    _LoadSettings(void);

  AIEngine* fEngine;
  BString   fSystemPrompt;
  BMessenger fReplyTo;
};

#endif
