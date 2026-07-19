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

#include "MoeAIClient.h"
#include "MoeDefs.h"
#include <ai/AICommands.h>
#include <ai/AIConfig.h>
#include <File.h>
#include <Alert.h>
#include <Catalog.h>

#include <cstdio>
#include <cstring>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AIClient"

static const char* kDefaultSystemPrompt =
  "You are Moeca, a cute crab and helpful desktop mascot living on Haiku OS "
  "along with a bunch of pirates: the Pirates of Frico. "
  "You are an AI assistant. You can help the user with "
  "questions and also control their Haiku system using the tools available to you. "
  "Be concise, friendly, and helpful. "
  "Respond in the same language the user writes to you. "
  "IMPORTANT: Always start your response with a language tag like [lang:it] or [lang:en] "
  "indicating the language you are responding in (ISO 639-1 code). "
  "This tag will be used for text-to-speech and will not be shown to the user.";

static MoeAIClient* sClient = NULL;


MoeAIClient*
MoeAIClient::Client(void)
{
  if (!sClient) {
    sClient = new MoeAIClient();
    sClient->Run();
  }
  return sClient;
}


MoeAIClient::MoeAIClient(void)
  : BLooper("MoeAIClient"),
    fSystemPrompt(kDefaultSystemPrompt),
    fEngine(NULL)
{
  _LoadSettings();
  fEngine = new AIEngine();
  if (fEngine != NULL) {
    fEngine->SetSystemPrompt(fSystemPrompt.String());
  }
}


MoeAIClient::~MoeAIClient(void)
{
  delete fEngine;
}


static BString
_ReadConfigString(const char* filename, const char* defaultVal)
{
  BString path(MOE_CONFIG_DIRECTORY);
  path << filename;
  BFile file(path.String(), B_READ_ONLY);
  if (file.InitCheck() != B_OK)
    return BString(defaultVal);

  off_t size;
  file.GetSize(&size);
  if (size <= 0 || size > 4096)
    return BString(defaultVal);

  BString result;
  char* buf = result.LockBuffer(size + 1);
  file.Read(buf, size);
  buf[size] = '\0';
  result.UnlockBuffer(size);
  result.Trim();

  if (result.Length() == 0)
    return BString(defaultVal);
  return result;
}


void
MoeAIClient::_LoadSettings(void)
{
  fSystemPrompt = _ReadConfigString("system_prompt", kDefaultSystemPrompt);
}


void
MoeAIClient::ReloadSettings(void)
{
  _LoadSettings();
  if (fEngine != NULL) {
    fEngine->SetSystemPrompt(fSystemPrompt.String());
  }
}


bool
MoeAIClient::HasApiKey(void) const
{
  // L'API key è gestita direttamente a livello globale dal server AI di Haiku,
  // quindi ritorniamo sempre true per non bloccare l'interfaccia di Moe.
  return true;
}


void
MoeAIClient::SendChat(const char* userText, BMessenger replyTo)
{
  BMessage* msg = new BMessage(MOE_CLAUDE_REQUEST);
  msg->AddString("text", userText);
  msg->AddMessenger("reply_to", replyTo);
  PostMessage(msg);
}


void
MoeAIClient::ClearHistory(void)
{
  BMessage msg(MOE_CHAT_CLEAR);
  PostMessage(&msg);
}


void
MoeAIClient::MessageReceived(BMessage* msg)
{
  switch (msg->what) {
    case MOE_CLAUDE_REQUEST:
    {
      const char* userText;
      BMessenger replyTo;
      if (msg->FindString("text", &userText) != B_OK
          || msg->FindMessenger("reply_to", &replyTo) != B_OK)
        break;

      fReplyTo = replyTo;

      if (fEngine == NULL) {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error", B_TRANSLATE("AI Engine not initialized."));
        fReplyTo.SendMessage(&errMsg);
        break;
      }

      // Avvia la generazione asincrona tramite l'AIEngine di Haiku.
      // Le risposte arriveranno a questo Looper sotto forma di MSG_AI_RESPONSE o MSG_AI_ERROR.
      status_t err = fEngine->GenerateAsync(userText, BMessenger(this));
      if (err != B_OK) {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error", B_TRANSLATE("Failed to send prompt to Haiku AI Server."));
        fReplyTo.SendMessage(&errMsg);
      }
      break;
    }

    case MSG_AI_RESPONSE:
    {
      bool complete = false;
      msg->FindBool("complete", &complete);

      if (complete) {
        const char* responseText = NULL;
        if (msg->FindString("response", &responseText) == B_OK && responseText != NULL) {
          BMessage reply(MOE_CHAT_RESPONSE);
          reply.AddString("text", responseText);
          fReplyTo.SendMessage(&reply);
        } else {
          BMessage errMsg(MOE_CHAT_ERROR);
          errMsg.AddString("error", B_TRANSLATE("Received empty response from AI Server."));
          fReplyTo.SendMessage(&errMsg);
        }
      }
      break;
    }

    case MSG_AI_ERROR:
    {
      const char* errorText = NULL;
      if (msg->FindString("error", &errorText) == B_OK && errorText != NULL) {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error", errorText);
        fReplyTo.SendMessage(&errMsg);
      } else {
        BMessage errMsg(MOE_CHAT_ERROR);
        errMsg.AddString("error", B_TRANSLATE("An unknown error occurred in the AI Server."));
        fReplyTo.SendMessage(&errMsg);
      }
      break;
    }

    case MOE_CHAT_CLEAR:
    {
      // Distruggendo e ricreando l'AIEngine, forziamo l'apertura di una nuova sessione
      // con un context_id differente (generato dal server in base al real_time_clock),
      // ripulendo di fatto la cronologia locale della conversazione corrente.
      delete fEngine;
      fEngine = new AIEngine();
      if (fEngine != NULL) {
        fEngine->SetSystemPrompt(fSystemPrompt.String());
      }
      break;
    }

    default:
      BLooper::MessageReceived(msg);
      break;
  }
}
