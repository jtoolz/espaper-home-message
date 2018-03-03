
#include <JsonListener.h>
#include <JsonStreamingParser.h>

typedef struct HomeMsgFields {
  String content;
  String senderName;
  String senderPhone;
  String created;
} HomeMsgFields;

class HomeMessage: public JsonListener {
  private:
    static const String _APIRequestURL;
    String currentKey;
    String currentParent;
    HomeMsgFields *fields;


  void doUpdate(HomeMsgFields *fields);

  public:
    HomeMessage();
    void updateMessage(HomeMsgFields *fields);
    
    virtual void whitespace(char c);

    virtual void startDocument();

    virtual void key(String key);

    virtual void value(String value);

    virtual void endArray();

    virtual void endObject();

    virtual void endDocument();

    virtual void startArray();

    virtual void startObject();
};