# Nya SMTP
Simple SMTP library for QT 4.8 / 5.*

This library is based on libqxt v.0.6.2.  
It has nothing but SMTP functionality.  
It has no dependencies except QT.  
But it uses C++11.

## Example
``` c++
#include "SmtpNya.hpp"
#include "MailNya.hpp"
#include "AttachmentNya.hpp"

...

Nya::Smtp smtp("smtp.yandex.ru", "login@yandex.ru", "password");
// QObject::connect(&smtp, SIGNAL(SignalError(QString)), &Nya::Log::GS(), SLOT(OnLog(QString)));
smtp.Connect();

// first simple mail
s_p<Nya::Mail> mail(new Nya::Mail("login@yandex.ru", "Subject", "Mail body"));
mail->AddRecipient("login@gmail.com");
mail->AddAttachment("file.txt");
smtp.Send(mail);

// second mail with QByteArray and QFile attachments
QByteArray ba = "Text 2";
QFile* file = new QFile("file.txt");

mail.reset(new Nya::Mail("login@yandex.ru", "Subject 2"));
mail->AddRecipient("login@gmail.com");
mail->SetText("Long mail body");
mail->AddAttachment("ba.txt", new Nya::Attachment(&ba));
mail->AddAttachment("file.txt", new Nya::Attachment(file));
smtp.Send(mail);
```
