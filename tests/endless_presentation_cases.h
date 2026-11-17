#ifndef ZEDAS_ENDLESS_PRESENTATION_CASES_H
#define ZEDAS_ENDLESS_PRESENTATION_CASES_H

static std::string savedAttributes(const Item& item)
{
    PropWriteStream stream;
    item.serializeAttr(stream);
    size_t size;
    const char* data = stream.getStream(size);
    return std::string(data, size);
}

static void testEndlessPresentation(bool active)
{
    struct Example { uint16_t id, aid, subtype; const char* name; };
    const Example examples[] = {
        {3160,65001,3,"endless ultimate healing rune"},
        {3191,65002,3,"endless great fireball rune"},
        {3200,65003,3,"endless explosion rune"},
        {2874,65004,10,"endless mana fluid"},
        {3046,65006,1,"endless magic light wand"},
        {3047,65006,1,"endless magic light wand"}
    };
    for (const auto& example : examples) {
        Item* marked=Item::CreateItem(example.id,example.subtype);
        Item* ordinary=Item::CreateItem(example.id,example.subtype);
        marked->setActionId(example.aid);
        const auto attributes=savedAttributes(*marked);
        const std::string originalName=marked->getName();
        const std::string text=marked->getDescription(1);
        if (active) {
            require(marked->getNameDescription()==std::string("an ")+example.name,"exact endless Look name/article");
            require(Item::getNameDescription(Item::items[example.id],marked,-1,false)==example.name,"name without article");
            require(text.find(example.name)!=std::string::npos && marked->getDescription(5).find(example.name)!=std::string::npos,
                    "endless items identifiable nearby and at distance");
            if (example.id==3160 || example.id==3191 || example.id==3200) {
                require(text.find("endless uses")!=std::string::npos && text.find("3x")==std::string::npos,
                        "endless rune does not claim a finite charge count");
                require(text.find("for magic level")!=std::string::npos,"normal rune requirements stay visible");
                require(ordinary->getDescription(1).find("3x")!=std::string::npos,"ordinary rune retains historical charge text");
            }
        } else {
            require(text==ordinary->getDescription(1) && marked->getNameDescription()==ordinary->getNameDescription(),
                    "master/feature disabled: byte-identical historical Look text");
        }
        require(savedAttributes(*marked)==attributes && marked->getName()==originalName,
                "Look text never rewrites attributes or underlying item name");
        require(ordinary->getDescription(1).find("endless")==std::string::npos,"ordinary item never labeled endless");
        marked->setActionId(65005);
        require(marked->getDescription(1)==ordinary->getDescription(1),"wrong item/AID pair keeps ordinary description");
        marked->setActionId(example.aid);
        PropStream stream; stream.init(attributes.data(),attributes.size());
        Item* restored=Item::CreateItem(example.id,example.subtype);
        require(restored->unserializeAttr(stream) && restored->getDescription(1)==text,"existing persisted marking needs no name migration");
        marked->setSpecialDescription("Existing owner inscription.");
        require(marked->getDescription(1).find("Existing owner inscription.")!=std::string::npos,"custom inscription preserved");
        marked->decrementReferenceCounter(); ordinary->decrementReferenceCounter(); restored->decrementReferenceCounter();
    }
    for (uint16_t subtype : {uint16_t(0),uint16_t(11)}) {
        Item* fluid=Item::CreateItem(2874,subtype); fluid->setActionId(65004);
        require(fluid->getDescription(1).find("endless")==std::string::npos,"wrong fluid subtype never labeled endless");
        fluid->decrementReferenceCounter();
    }
    for (uint16_t id : {uint16_t(3046),uint16_t(3047)}) {
        Item* marked=Item::CreateItem(id); marked->setActionId(65006);
        Item* ordinary=Item::CreateItem(id);
        marked->setDuration(900000); ordinary->setDuration(900000);
        const auto initial=marked->getDescription(1);
        marked->setDuration(1000); ordinary->setDuration(1000);
        if(active) {
            require(initial==marked->getDescription(1) && initial.find("endless light while lit")!=std::string::npos &&
                    initial.find("minutes left")==std::string::npos,"lit and paused endless wands never show decreasing energy text");
        } else require(marked->getDescription(1)==ordinary->getDescription(1),"disabled wand retains native timer text");
        require(ordinary->getDescription(1).find("less than a minute left")!=std::string::npos,"ordinary wand countdown unchanged");
        require(marked->getDuration()==1000,"presentation does not freeze the actual decay timer");
        marked->decrementReferenceCounter(); ordinary->decrementReferenceCounter();
    }
    require(Item::getDescription(Item::items[3047],1).find("endless")==std::string::npos,"item-type-only look cannot acquire a mark");
}
#endif
