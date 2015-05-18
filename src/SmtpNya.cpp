#include "MailNya.hpp"

#include <QCryptographicHash>
#include <QStringList>
#include <QTcpSocket>
#include <QThread>
#include <QNetworkInterface>
#ifndef QT_NO_OPENSSL
#    include <QSslSocket>
#endif

#include "SmtpNya.hpp"


namespace Nya
{
static QByteArray ExtractAddress(const QString& address)
{
	int parenDepth = 0;
	int addrStart = -1;
	bool inQuote = false;
	int ct = address.length();

	for (int i = 0; i < ct; i++)
	{
		QChar ch = address[i];
		if (inQuote)
		{
			if (ch == '"')
				inQuote = false;
		}
		else if (addrStart != -1)
		{
			if (ch == '>')
				return address.mid(addrStart, (i - addrStart)).toLatin1();
		}
		else if (ch == '(')
		{
			parenDepth++;
		}
		else if (ch == ')')
		{
			parenDepth--;
			if (parenDepth < 0) parenDepth = 0;
		}
		else if (ch == '"')
		{
			if (parenDepth == 0)
				inQuote = true;
		}
		else if (ch == '<')
		{
			if (!inQuote && parenDepth == 0)
				addrStart = i + 1;
		}
	}
	return address.toLatin1();
}

//===============================================================================
class Hmac
{
	typedef QCryptographicHash::Algorithm Algorithm;

	QCryptographicHash ohash;
	QCryptographicHash ihash;
	Algorithm algorithm;
	QByteArray opad, ipad, result;

public:
	Hmac(Algorithm algorithm)
		: ohash(algorithm)
		, ihash(algorithm)
		, algorithm(algorithm)
	{}

	void setKey(QByteArray key);
	void reset();

	void addData(const char* data, int length);
	void addData(const QByteArray& data);

	QByteArray innerHash() const;
	QByteArray GetResult();
	bool verify(const QByteArray& otherInner);

	static QByteArray hash(const QByteArray& key, const QByteArray& data, Algorithm algorithm);
};

/**
 * Sets the shared secret key for the message authentication code.
 *
 * Any data that had been processed using addData() will be discarded.
 */
void Hmac::setKey(QByteArray key)
{
	opad = QByteArray(64, 0x5c);
	ipad = QByteArray(64, 0x36);
	if (key.size() > 64)
	{
		key = QCryptographicHash::hash(key, algorithm);
	}
	for (int i = key.size() - 1; i >= 0; --i)
	{
		opad[i] = opad[i] ^ key[i];
		ipad[i] = ipad[i] ^ key[i];
	}
	reset();
}

/**
 * Resets the object.
 *
 * Any data that had been processed using addData() will be discarded.
 * The key, if set, will be preserved.
 */
void Hmac::reset()
{
	ihash.reset();
	ihash.addData(ipad);
}

/**
 * Returns the inner hash of the HMAC function.
 *
 * This hash can be stored in lieu of the shared secret on the authenticating side
 * and used for verifying an HMAC code. When used in this manner, HMAC can be used
 * to provide a form of secure password authentication. See the documentation above
 * for details.
 */
QByteArray Hmac::innerHash() const
{
	return ihash.result();
}

/**
 * Returns the authentication code for the message.
 */
QByteArray Hmac::GetResult()
{
	if( result.size() ) return result;
	ohash.reset();
	ohash.addData(opad);
	ohash.addData(innerHash());
	result = ohash.result();
	return result;
}

/**
 * Verifies the authentication code against a known inner hash.
 */
bool Hmac::verify(const QByteArray& otherInner)
{
	GetResult();
	ohash.reset();
	ohash.addData(opad);
	ohash.addData(otherInner);
	return result == ohash.result();
}

/**
 * Adds the provided data to the message to be authenticated.
 */
void Hmac::addData(const char* data, int length)
{
	ihash.addData(data, length);
	result.clear();
}

/**
 * Adds the provided data to the message to be authenticated.
 */
void Hmac::addData(const QByteArray& data)
{
	addData(data.constData(), data.size());
}

/**
 * Returns the HMAC of the provided data using the specified key and hashing algorithm.
 */
QByteArray Hmac::hash(const QByteArray& key, const QByteArray& data, Algorithm algorithm)
{
	Hmac hmac(algorithm);
	hmac.setKey(key);
	hmac.addData(data);
	return hmac.GetResult();
}

//==============================================================================
Smtp::Smtp(const QString& host, const QByteArray& username, const QByteArray& password, QObject* parent)
	: QObject(parent)
	, host(host)
	, username(username)
	, password(password)
	, allowedAuthTypes(AuthPlain | AuthLogin | AuthCramMD5)
	, defaultSender(username)
{
#ifndef QT_NO_OPENSSL
	socket = new QSslSocket(this);
#else
	socket = new QTcpSocket(this);
#endif
	connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(OnSocketError(QAbstractSocket::SocketError)));
	connect(socket, SIGNAL(readyRead()), SLOT(OnSocketRead()));

	if( !parent )
	{
		QThread* thread = new QThread(this);
		moveToThread(thread);
		thread->start();
	}
}

/**
 * Connect to host.
 */
void Smtp::Connect()
{
	if( state != Disconnected ) return;

	state = StartState;
#ifndef QT_NO_OPENSSL
	((QSslSocket*)socket)->connectToHostEncrypted(host, port);
#else
	socket->connectToHost(host, port);
#endif
}

/**
 * Disconnect from host.
 */
void Smtp::Disconnect()
{
	socket->disconnectFromHost();
}

/**
 * Send mail.
 */
void Smtp::Send(s_p<Mail> mail)
{
	pending.append(mail);
	if( state == Waiting ) SendNext();
}

/**
 * Parse ehlo.
 */
void Smtp::ParseEhlo(const QByteArray& code, bool cont, const QString& line)
{
	if( code != "250" )
	{
		// error!
		if (state != HeloSent)
		{
			// maybe let's try HELO
			socket->write("helo\r\n");
			state = HeloSent;
		}
		else
		{
			// nope
			socket->write("QUIT\r\n");
			socket->flush();
			socket->disconnectFromHost();
		}
		return;
	}
	else if( state != EhloGreetReceived )
	{
		if( !cont )
		{
			// greeting only, no extensions
			state = EhloDone;
		}
		else
		{
			// greeting followed by extensions
			state = EhloGreetReceived;
			return;
		}
	}
	else
	{
		extensions[line.section(' ', 0, 0).toUpper()] = line.section(' ', 1);
		if( !cont ) state = EhloDone;
	}
	if (state != EhloDone) return;
	if (extensions.contains("STARTTLS")) StartTLS();
	else Authenticate();
}

/**
 * TLS.
 */
void Smtp::StartTLS()
{
#ifndef QT_NO_OPENSSL
	socket->write("starttls\r\n");
	state = StartTLSSent;
#else
	Authenticate();
#endif
}

/**
 * Authenticate.
 */
void Smtp::Authenticate()
{
	if (!extensions.contains("AUTH") || username.isEmpty() || password.isEmpty())
	{
		state = Authenticated;
		SendNext();
	}
	else
	{
		QStringList auth = extensions["AUTH"].toUpper().split(' ', QString::SkipEmptyParts);
		if (auth.contains("CRAM-MD5") && (allowedAuthTypes & AuthCramMD5))
		{
			AuthenticateCramMD5();
		}
		else if (auth.contains("PLAIN") && (allowedAuthTypes & AuthPlain))
		{
			AuthenticatePlain();
		}
		else if (auth.contains("LOGIN") && (allowedAuthTypes & AuthLogin))
		{
			AuthenticateLogin();
		}
		else
		{
			state = Authenticated;
			SendNext();
		}
	}
}

/**
 * CramMD5.
 */
void Smtp::AuthenticateCramMD5(const QByteArray& challenge)
{
	if (state != AuthRequestSent)
	{
		socket->write("auth cram-md5\r\n");
		authType = AuthCramMD5;
		state = AuthRequestSent;
	}
	else
	{
		Hmac hmac(QCryptographicHash::Md5);
		hmac.setKey(password);
		hmac.addData(QByteArray::fromBase64(challenge));
		QByteArray response = username + ' ' + hmac.GetResult().toHex();
		socket->write(response.toBase64() + "\r\n");
		state = AuthSent;
	}
}

/**
 * Plain.
 */
void Smtp::AuthenticatePlain()
{
	if (state != AuthRequestSent)
	{
		socket->write("auth plain\r\n");
		authType = AuthPlain;
		state = AuthRequestSent;
	}
	else
	{
		QByteArray auth;
		auth += '\0';
		auth += username;
		auth += '\0';
		auth += password;
		socket->write(auth.toBase64() + "\r\n");
		state = AuthSent;
	}
}

/**
 * Login.
 */
void Smtp::AuthenticateLogin()
{
	if (state != AuthRequestSent && state != AuthUsernameSent)
	{
		socket->write("auth login\r\n");
		authType = AuthLogin;
		state = AuthRequestSent;
	}
	else if (state == AuthRequestSent)
	{
		socket->write(username.toBase64() + "\r\n");
		state = AuthUsernameSent;
	}
	else
	{
		socket->write(password.toBase64() + "\r\n");
		state = AuthSent;
	}
}

/**
 * Send next mail.
 */
void Smtp::SendMail(const QByteArray& code, const QByteArray& line)
{
	if( !pending.count() ) return;
	s_p<Mail> mail = pending.first();

	if (code[0] != '2')
	{
		// on failure, emit a warning signal
		if (!mailAck)
		{
			emit SignalError(QString("Sender rejected: %1 - %2").arg(QString(line)).arg(mail->GetSender()));
		}
		else
		{
			emit SignalError(QString("Recipient rejected: %1 - %2").arg(QString(line)).arg(mail->GetSender()));
		}
	}
	else if (!mailAck)
	{
		mailAck = true;
	}
	else
	{
		rcptAck++;
	}

	if (rcptNumber == recipients.count())
	{
		// all recipients have been sent
		if (rcptAck == 0)
		{
			emit SignalError(QString("No recipients were considered valid: %1 - %2").arg(QString(line)).arg(code.toInt()));
			pending.removeFirst();
			SendNext();
		}
		else
		{
			// at least one recipient was acknowledged, send mail body
			socket->write("data\r\n");
			state = SendingBody;
		}
	}
	else if (state != RcptAckPending)
	{
		// send the next recipient unless we're only waiting on acks
		socket->write("rcpt to:<" + ExtractAddress(recipients[rcptNumber]) + ">\r\n");
		rcptNumber++;
	}
	else
	{
		// If we're only waiting on acks, just count them
		rcptNumber++;
	}
}

/**
 * Send body.
 */
void Smtp::SendBody(const QByteArray& code, const QByteArray& line)
{
	s_p<Mail> mail = pending.first();

	if (code[0] != '3')
	{
		emit SignalError(QString("Mail failed: %1 - %2").arg(QString(line)).arg(code.toInt()));
		pending.removeFirst();
		SendNext();
		return;
	}

	socket->write(*mail);
	socket->write(".\r\n");
	state = BodySent;
}

/**
 * Send ehlo.
 */
void Smtp::SendEhlo()
{
	QByteArray address = "127.0.0.1";
	foreach(const QHostAddress& addr, QNetworkInterface::allAddresses())
	{
		if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6)
			continue;
		address = addr.toString().toLatin1();
		break;
	}
	socket->write("ehlo " + address + "\r\n");
	extensions.clear();
	state = EhloSent;
}

/**
 * Send.
 */
void Smtp::SendNext()
{
	if (state == Disconnected) return;
	if (pending.isEmpty())
	{
		state = Waiting;
		return;
	}
	if (state != Waiting)
	{
		state = Resetting;
		socket->write("rset\r\n");
		return;
	}
	s_p<Mail> mail = pending.first();
	rcptNumber = rcptAck = mailAck = 0;
	recipients = mail->GetRecipients(R_TO) +
				 mail->GetRecipients(R_CC) +
				 mail->GetRecipients(R_BCC);
	if (recipients.count() == 0)
	{
		emit SignalError("No recipients!");
		pending.removeFirst();
		SendNext();
		return;
	}
	socket->write("mail from:<" + ExtractAddress(mail->GetSender()) + ">\r\n");
	if (extensions.contains("PIPELINING"))
	{
		for (const QString& recipient : recipients)
		{
			socket->write("rcpt to:<" + ExtractAddress(recipient) + ">\r\n");
		}
		state = RcptAckPending;
	}
	else
	{
		state = MailToSent;
	}
}

/**
 * Socket error.
 */
void Smtp::OnSocketError(QAbstractSocket::SocketError err)
{
	if( err == QAbstractSocket::RemoteHostClosedError )
	{
		state = Disconnected;
		return;
	}

	emit SignalError(QString("Socket error [%1]: %2").arg(int(err)).arg(socket->errorString()));
}

/**
 * Read.
 */
void Smtp::OnSocketRead()
{
	buffer += socket->readAll();
	while (true)
	{
		int pos = buffer.indexOf("\r\n");
		if (pos < 0) return;
		QByteArray line = buffer.left(pos);
		buffer = buffer.mid(pos + 2);
		QByteArray code = line.left(3);
		switch (state)
		{
		case StartState:
			if (code[0] != '2')
			{
				state = Disconnected;
				emit SignalError(QString("Connection failed: ") + line);
				socket->disconnectFromHost();
			}
			else
			{
				SendEhlo();
			}
			break;
		case HeloSent:
		case EhloSent:
		case EhloGreetReceived:
			ParseEhlo(code, (line[3] != ' '), line.mid(4));
			break;
#ifndef QT_NO_OPENSSL
		case StartTLSSent:
			if (code == "220")
			{
				socket->startClientEncryption();
				SendEhlo();
			}
			else
			{
				Authenticate();
			}
			break;
#endif
		case AuthRequestSent:
		case AuthUsernameSent:
			if (authType == AuthPlain) AuthenticatePlain();
			else if (authType == AuthLogin) AuthenticateLogin();
			else AuthenticateCramMD5(line.mid(4));
			break;
		case AuthSent:
			if (code[0] == '2')
			{
				state = Authenticated;
				SendNext();
			}
			else
			{
				state = Disconnected;
				emit SignalError(QString("Authentication failed: ") + line);
				socket->disconnectFromHost();
			}
			break;
		case MailToSent:
		case RcptAckPending:
			if (code[0] != '2') {
				emit SignalError(QString("Mail failed 2: %1 - %2").arg(QString(line)).arg(code.toInt()));
				SendNext();
				state = BodySent;
			}
			else SendMail(code, line);
			break;
		case SendingBody:
			SendBody(code, line);
			break;
		case BodySent:
			if ( pending.count() )
			{
				if (code[0] != '2')
				{
					emit SignalError(QString("Mail failed 3: %1 - %2").arg(QString(line)).arg(code.toInt()));
				}
				else
				{
					emit SignalDone(pending.first());
				}
				pending.removeFirst();
				if( pending.count() == 0 ) emit SignalAllDone();
			}
			SendNext();
			break;
		case Resetting:
			if (code[0] != '2')
			{
				emit SignalError("Connection failed: " + line);
			}
			else
			{
				state = Waiting;
				SendNext();
			}
			break;
		default:;
		}
	}
}

/**
 * Single mail sending.
 * (default recipients must be set)
 */
void Smtp::OnMail(const QString& text, const QString& subject)
{
	Connect();
	s_p<Mail> mail(new Mail(defaultSender, subject.size() ? subject : defaultSubject, text));
	for( const auto& recipient : defaultRecipients ) mail->AddRecipient(recipient);

	Send(mail);
}

}
