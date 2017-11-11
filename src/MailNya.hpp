#ifndef MAILNYA_H
#define MAILNYA_H

#include "CommonMail.hpp"
#include <QHash>
#include <QStringList>


namespace Nya
{
enum RecipientType
{
	R_TO,
	R_CC,
	R_BCC
};

class Attachment;
class Mail
{
	friend class Rfc2822;

	QString sender, subject, text;
	QStringList rcptTo, rcptCc, rcptBcc;
	QHash<QByteArray, QByteArray> extraHeaders;
	QHash<QString, s_p<Attachment>> attachments;
	int wordWrap = 78;
	bool isKeepIndentation = false;
	mutable QByteArray boundary;

public:
	Mail(const QString& sender, const QString& subject = "", const QString& text = "");
	Mail(const QByteArray& rfc2822);
	virtual ~Mail();

	QString GetSender() const { return sender; }
	QString GetSubject() const { return subject; }
	QStringList GetRecipients(RecipientType type = R_TO) const;
	QHash<QByteArray, QByteArray> ExtraHeaders() const { return extraHeaders; }
	QHash<QString, s_p<Attachment>> GetAttachments() const { return attachments; }

	void SetText(const QString& text) { this->text = text; }
	void SetWordWrapLimit(int wordWrap) { this->wordWrap = wordWrap; }
	void SetKeepIndentation(bool isOn) { isKeepIndentation = isOn; }

	void AddRecipient(const QString& recipient, RecipientType type = R_TO);
	void RemoveRecipient(const QString& recipient);
	void AddExtraHeader(const QByteArray& key, const QByteArray& value);
	void RemoveExtraHeader(const QByteArray& key);
	void AddAttachment(const QString& filePath);
	void AddAttachment(const QString& fileName, Attachment* pA);
	void RemoveAttachment(const QString& filename);

	operator QByteArray() const; // to rfc2822
};

}

#endif // MAILNYA_H
