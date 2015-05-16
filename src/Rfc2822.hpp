#ifndef RFC2822_HPP
#define RFC2822_HPP

#include "CommonMail.hpp"
#include <QByteArray>
#include <QStringList>


namespace Nya
{
class Mail;
class Attachment;

class Rfc2822
{
	Mail* mail;
	QByteArray currentHeaderKey;
	QStringList currentHeaderValue;

public:
	Rfc2822(Mail* mail) : mail(mail) {}

	void Parse(const QByteArray& buffer);

private:
	void ParseHeader(const QByteArray& line, QHash<QByteArray, QByteArray>& headers);
	void ParseBody();
	void ParseEntity(const QByteArray& buffer, QHash<QByteArray, QByteArray>& headers, QString& body);
	s_p<Attachment> ParseAttachment(const QHash<QByteArray, QByteArray>& headers, const QString& body, QString& filename);
	QString UnfoldValue(QStringList& folded);
	QString Decode(const QString& charset, const QString& encoding, const QString& encoded);
};
}

#endif // RFC2822_HPP
