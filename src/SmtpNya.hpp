#ifndef SMTPNYA_H
#define SMTPNYA_H

#include "CommonMail.hpp"
#include <QAbstractSocket>
#include <QHash>
#include <QList>
#include <QPair>
#include <QStringList>


class QTcpSocket;
#ifndef QT_NO_OPENSSL
class QSslSocket;
#endif

namespace Nya
{
enum SmtpState
{
	Disconnected,
	StartState,
	EhloSent,
	EhloGreetReceived,
	EhloExtensionsReceived,
	EhloDone,
	HeloSent,
	StartTLSSent,
	AuthRequestSent,
	AuthUsernameSent,
	AuthSent,
	Authenticated,
	MailToSent,
	RcptAckPending,
	SendingBody,
	BodySent,
	Waiting,
	Resetting
};

enum SmtpError
{
	NoError,
	NoRecipients,
	CommandUnrecognized = 500,
	SyntaxError,
	CommandNotImplemented,
	BadSequence,
	ParameterNotImplemented,
	MailboxUnavailable = 550,
	UserNotLocal,
	MessageTooLarge,
	InvalidMailboxName,
	TransactionFailed
};

enum AuthType
{
	AuthPlain,
	AuthLogin,
	AuthCramMD5
};


class Mail;
class Smtp : public QObject
{
	Q_OBJECT

	QString host;
	QByteArray username, password;
	QByteArray buffer;
	SmtpState state = Disconnected;
	AuthType authType;
	int allowedAuthTypes;

	QByteArray defaultSender;
	QStringList defaultRecipients;
	QString defaultSubject;

	QStringList recipients;
	QHash<QString, QString> extensions;
	QList<s_p<Mail>> pending;
	int rcptNumber;
	int rcptAck;
	bool mailAck;

#ifndef QT_NO_OPENSSL
	QSslSocket* socket;
	quint16 port = 465;
#else
	QTcpSocket* socket;
	quint16 port = 25;
#endif

public:
	Smtp(const QString& host, const QByteArray& username, const QByteArray& password, QObject* parent = 0);

	QTcpSocket* GetSocket() const { return (QTcpSocket*)socket; }
	bool HasExtension(const QString& extension) { return extensions.contains(extension); }
	QString ExtensionData(const QString& extension) { return extensions[extension]; }
	bool IsAuthMethodEnabled(AuthType type) const { return allowedAuthTypes & type; }

	void SetPort(quint16 port) { this->port = port; }
	void SetAuthMethodEnabled(AuthType type, bool enable) { if( enable ) allowedAuthTypes |= type; else allowedAuthTypes &= ~type; }
	void SetSender(const QByteArray& sender) { defaultSender = sender; }
	void SetRecipients(const QStringList recipients) { defaultRecipients = recipients; }
	void SetSubject(const QString& subject) { defaultSubject = subject; }

	void Connect();
	void Disconnect();
	void Send(s_p<Mail> mail);

private:
	void ParseEhlo(const QByteArray& code, bool cont, const QString& line);
	void StartTLS();
	void Authenticate();

	void AuthenticateCramMD5(const QByteArray& challenge = QByteArray());
	void AuthenticatePlain();
	void AuthenticateLogin();

	void SendMail(const QByteArray& code, const QByteArray& line);
	void SendBody(const QByteArray& code, const QByteArray& line);
	void SendEhlo();
	void SendNext();

private slots:
	void OnSocketError(QAbstractSocket::SocketError err);
	void OnSocketRead();

	void OnMail(const QString& text, const QString& subject = "");

signals:
	void SignalError(const QString& message);
	void SignalDone(s_p<Mail> mail);
	void SignalAllDone();
};
}

#endif // SMTPNYA_H
