# Nya SMTP
*Simple SMTP library for QT 4.8 / 5.\**

It is based on libqxt v.0.6.2.  

! OAUTH is not supported, so this library can't send from gmail without changing gmail security settings.

## Example 1
``` c++
#include "SmtpNya.hpp"

...

auto smtp = new Nya::Smtp("smtp.mail1.com", "s@mail1.com", "password");
smtp->SetSubject("Error report for App");
smtp->SetRecipients(QStringList() << "r1@mail2.com" << "r2@mail3.com");

connect(this, SIGNAL(SignalMail(QString)), smtp, SLOT(OnMail(QString)));

...

emit SignalMail("Mail body");
```

## Example 2
``` c++
#include "SmtpNya.hpp"
#include "MailNya.hpp"
#include "AttachmentNya.hpp"

...

Nya::Smtp smtp("smtp.mail1.com", "s@mail1.com", "password");
// QObject::connect(&smtp, SIGNAL(SignalError(QString)), &Nya::Log::GS(), SLOT(OnLog(QString)));
smtp.Connect();

// first simple mail
s_p<Nya::Mail> mail(new Nya::Mail("s@mail1.com", "Subject", "Mail body"));
mail->AddRecipient("r@mail2.com");
mail->AddAttachment("file.txt");
smtp.Send(mail);

// second mail with QByteArray and QFile attachments
QByteArray ba = "Text 2";
QFile* file = new QFile("file.txt");

mail.reset(new Nya::Mail("s@mail1.com", "Subject 2"));
mail->AddRecipient("r@mail2.com");
mail->SetText("Long mail body");
mail->AddAttachment("ba.txt", new Nya::Attachment(&ba));
mail->AddAttachment("file.txt", new Nya::Attachment(file));
smtp.Send(mail);
```
