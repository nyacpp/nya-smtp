# Nya SMTP
Simple SMTP library for QT 4.8 / 5.*

This library is based on libqxt v.0.6.2.  
It has nothing but SMTP functionality.  
It has no dependencies except QT.  
But it uses C++11.

## Example
``` c++
Nya::Smtp smtp("smtp.yandex.ru", "login@yandex.ru", "password");
QObject::connect(&smtp, SIGNAL(SignalError(QString)), &Nya::Log::GS(), SLOT(OnLog(QString)));
smtp.Connect();

s_p<Nya::Mail> mail(new Nya::Mail("login@yandex.ru", "Subject", "Message body"));
mail->AddRecipient("login@gmail.com");
mail->AddAttachment("file.txt");
smtp.Send(mail);
```
