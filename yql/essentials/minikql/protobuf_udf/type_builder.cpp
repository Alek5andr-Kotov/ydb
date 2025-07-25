#include "type_builder.h"

#include <yql/essentials/public/udf/udf_value.h>
#include <yql/essentials/public/udf/udf_type_inspection.h>
#include <yql/essentials/utils/yql_panic.h>

#include <yt/cpp/mapreduce/interface/protobuf_format.h>
#include <yt/yt_proto/yt/formats/extension.pb.h>

#include <util/generic/set.h>

#include <optional>

namespace NYql {
namespace NUdf {
namespace {

using namespace NProtoBuf;

class TTypeBuilder {
public:
     TTypeBuilder(EEnumFormat enumFormat,
                  ERecursionTraits recursion,
                  bool ytMode,
                  bool optionalLists,
                  bool syntaxAware,
                  bool useJsonName,
                  EProtoStringYqlType stringType,
                  IFunctionTypeInfoBuilder& builder);
    ~TTypeBuilder();

    void Build(const Descriptor* descriptor, TProtoInfo* info);

    TType* GenerateTypeInfo(const Descriptor* descriptor, bool defaultYtSerialize);

private:
    TType* GetBytesType();
    TType* GetInt64Type();
    TType* GetYsonType();
    // Get type without optional/repeated/required modifier.
    TType* GetUnderlyingType(const FieldDescriptor* fd, bool defaultYtSerialize);
    // Wrap |type| based on modifiers.
    TType* WrapTypeFromModifiers(TType* type,
                                 const FieldDescriptor* fd,
                                 EFieldContext fieldContext,
                                 const std::optional<NYT::NDetail::TProtobufFieldOptions>& ytOpts,
                                 TFlags<EFieldFlag>& flags,
                                 bool recursive);
    TType* GetOptionalType(TType* type);
    TType* GetListType(TType* type, const std::optional<NYT::NDetail::TProtobufFieldOptions>& ytOpts, TFlags<EFieldFlag>& flags);

private:
    using TTypeMap = THashMap<TType*, TType*>;

    EEnumFormat  EnumFormat_;
    ERecursionTraits Recursion_;
    bool YtMode_;
    bool OptionalLists_;
    bool SyntaxAware_;
    bool UseJsonName_;
    EProtoStringYqlType StringType_;
    IFunctionTypeInfoBuilder&
                 Builder_;
    TProtoInfo*  Info_;
    TType*       BasicTypes_[FieldDescriptor::Type::MAX_TYPE + 1];
    TType*       YsonType_;
    TSet<TString> KnownMessages_;
    TTypeMap     Optionals_;
    TTypeMap     Lists_;
};

TTypeBuilder::TTypeBuilder(EEnumFormat enumFormat,
                           ERecursionTraits recursion,
                           bool ytMode,
                           bool optionalLists,
                           bool syntaxAware,
                            bool useJsonName,
                           EProtoStringYqlType stringType,
                           IFunctionTypeInfoBuilder& builder)
    : EnumFormat_(enumFormat)
    , Recursion_(recursion)
    , YtMode_(ytMode)
    , OptionalLists_(optionalLists)
    , SyntaxAware_(syntaxAware)
    , UseJsonName_(useJsonName)
    , StringType_(stringType)
    , Builder_(builder)
    , Info_(nullptr)
    , YsonType_(nullptr)
{
    for (size_t i = 0; i < Y_ARRAY_SIZE(BasicTypes_); ++i) {
        BasicTypes_[i] = nullptr;
    }
}

TTypeBuilder::~TTypeBuilder()
{ }

void TTypeBuilder::Build(const Descriptor* descriptor, TProtoInfo* info) {
    Info_ = info;
    Info_->EnumFormat = EnumFormat_;
    Info_->Recursion = Recursion_;
    Info_->YtMode = YtMode_;
    Info_->StructType = GenerateTypeInfo(descriptor, false);
    Info_->OptionalLists = OptionalLists_;
    Info_->SyntaxAware = SyntaxAware_;
    Info_->StringType = StringType_;
    Info_->UseJsonName = UseJsonName_;
}

TType* TTypeBuilder::GenerateTypeInfo(const Descriptor* descriptor, bool defaultYtSerialize) {
    auto fullName = descriptor->full_name();

    if (KnownMessages_.find(fullName) != KnownMessages_.end()) {
        auto mi = Info_->Messages.find(fullName);
        if (mi == Info_->Messages.end()) {
            switch (Recursion_) {
                case ERecursionTraits::Fail:
                    ythrow yexception() << "can't handle recursive types: "
                                        << fullName;
                case ERecursionTraits::Bytes:
                case ERecursionTraits::BytesV2:
                case ERecursionTraits::Ignore:
                    return nullptr;
            }
        }
        return mi->second->StructType;
    } else {
        KnownMessages_.insert(fullName);
    }

    std::shared_ptr<TMessageInfo> message = std::make_shared<TMessageInfo>();

    auto makeField = [&](ui32& pos, TFlags<EFieldFlag>& flags, const IStructTypeBuilder::TPtr& structType, const FieldDescriptor* fd, EFieldContext fieldContext) {
        auto name = fd->name();
        if (UseJsonName_) {
            Y_ASSERT(!fd->json_name().empty());
            name = fd->json_name();
        } else if (YtMode_) {
            name = NYT::NDetail::GetColumnName(*fd);
        }

        // Создаём тип поля
        TType* type = nullptr;
        std::optional<NYT::NDetail::TProtobufFieldOptions> ytOpts;
        if (YtMode_) {
            ytOpts = NYT::NDetail::GetFieldOptions(fd,
                                                   defaultYtSerialize
                                                       ? MakeMaybe<NYT::NDetail::TProtobufFieldOptions>(NYT::NDetail::TProtobufFieldOptions{.SerializationMode = NYT::NDetail::EProtobufSerializationMode::Yt})
                                                       : Nothing());
        }
        auto wrapRecursiveType = [&](TType* type, TFlags<EFieldFlag>& flags, bool wrapWithModifiers) {
            if (type) {
                return type;
            }
            if (Recursion_ == ERecursionTraits::Ignore) {
                flags |= EFieldFlag::Void;
                return Builder_.Void();
            }
            Y_ENSURE(Recursion_ == ERecursionTraits::Bytes || Recursion_ == ERecursionTraits::BytesV2);
            flags |= EFieldFlag::Binary;
            type = GetBytesType();
            if (!wrapWithModifiers) {
                return type;
            }
            return WrapTypeFromModifiers(type, fd, fieldContext, ytOpts, flags, /*recursive=*/true);
        };

        if (YtMode_) {
            if (fd->is_map()) {
                auto mapMessage = fd->message_type();
                switch (ytOpts->MapMode) {
                case NYT::NDetail::EProtobufMapMode::ListOfStructsLegacy:
                case NYT::NDetail::EProtobufMapMode::ListOfStructs:
                    type = GenerateTypeInfo(mapMessage, NYT::NDetail::EProtobufMapMode::ListOfStructs == ytOpts->MapMode);
                    break;
                case NYT::NDetail::EProtobufMapMode::Dict:
                case NYT::NDetail::EProtobufMapMode::OptionalDict:
                    Y_ENSURE(mapMessage->field_count() == 2);
                    flags |= EFieldFlag::Dict;
                    type = Builder_.Dict()
                        ->Key(GetUnderlyingType(mapMessage->map_key(), false))
                        .Value(wrapRecursiveType(GetUnderlyingType(mapMessage->map_value(), true), flags, /*wrapWithModifiers=*/false))
                        .Build();
                    message->DictTypes[fd->number()] = type;
                    if (NYT::NDetail::EProtobufMapMode::OptionalDict == ytOpts->MapMode) {
                        flags |= EFieldFlag::OptionalContainer;
                        type = GetOptionalType(type);
                    }
                    break;
                }
            } else if (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE && ytOpts->SerializationMode == NYT::NDetail::EProtobufSerializationMode::Protobuf) {
                type = GetBytesType();
                flags |= EFieldFlag::Binary;
            } else if (ytOpts->Type) {
                switch (*ytOpts->Type) {
                    case NYT::NDetail::EProtobufType::Any:
                        if (fd->type() != FieldDescriptor::TYPE_BYTES) {
                            ythrow yexception() << "Expected 'string' type of 'ANY' field: " << fd->name();
                        }
                        type = GetYsonType();
                        break;
                    case NYT::NDetail::EProtobufType::EnumInt:
                        if (fd->type() != FieldDescriptor::TYPE_ENUM) {
                            ythrow yexception() << "Expected 'enum' type of 'ENUM_INT' field: " << fd->name();
                        }
                        flags |= EFieldFlag::EnumInt;
                        type = GetInt64Type();
                        break;
                    case NYT::NDetail::EProtobufType::EnumString:
                        if (fd->type() != FieldDescriptor::TYPE_ENUM) {
                            ythrow yexception() << "Expected 'enum' type of 'ENUM_STRING' field: " << fd->name();
                        }
                        flags |= EFieldFlag::EnumString;
                        type = GetBytesType();
                        break;
                    default:
                        ythrow yexception() << "Unsupported YT extension tag: " << *ytOpts->Type
                                            << ", field: " << fd->name();
                }
            } else {
                type = GetUnderlyingType(fd, false);
            }
        } else {
            type = GetUnderlyingType(fd, false);
        }

        if (!flags.HasFlags(EFieldFlag::Dict)) {
            if (type) {
                type = WrapTypeFromModifiers(type, fd, fieldContext, ytOpts, flags, /*recursive=*/false);
            } else {
                type = wrapRecursiveType(type, flags, /*wrapWithModifiers=*/true);
            }
        }

        // Добавляем поле в текущую структуру
        structType->AddField(name, type, &pos);
    };

    auto structType = Builder_.Struct(descriptor->field_count());
    message->Fields.reserve(descriptor->field_count());

    THashMap<const OneofDescriptor*, ui32> visitedOneofs;
    for (int i = 0, end = descriptor->field_count(); i < end; ++i) {
        const FieldDescriptor* fd = descriptor->field(i);
        EFieldContext fieldContext = GetFieldContext(fd, YtMode_);
        if (auto oneofDescriptor = fd->containing_oneof(); YtMode_ && oneofDescriptor) {
            if (!visitedOneofs.contains(oneofDescriptor)) {
                auto oneofOptions = NYT::NDetail::GetOneofOptions(oneofDescriptor);
                switch (oneofOptions.Mode) {
                    case NYT::NDetail::EProtobufOneofMode::SeparateFields:
                        for (int i = 0; i < oneofDescriptor->field_count(); ++i) {
                            auto& field = message->Fields[oneofDescriptor->field(i)->number()];
                            Y_ENSURE(fieldContext == EFieldContext::InsideOneofYtSeparateFields);
                            makeField(field.Pos, field.Flags, structType, oneofDescriptor->field(i), fieldContext);
                        }
                        visitedOneofs.emplace(oneofDescriptor, Max<ui32>());
                        break;
                    case NYT::NDetail::EProtobufOneofMode::Variant: {
                        auto varStructType = Builder_.Struct(oneofDescriptor->field_count());
                        for (int i = 0; i < oneofDescriptor->field_count(); ++i) {
                            auto fdOneof = oneofDescriptor->field(i);
                            auto& field = message->Fields[fdOneof->number()];
                            Y_ENSURE(fieldContext == EFieldContext::InsideOneofYtVariant);
                            field.Flags |= EFieldFlag::Variant;
                            makeField(message->VariantIndicies[fdOneof->number()], field.Flags, varStructType, fdOneof, fieldContext);
                        }
                        structType->AddField(oneofOptions.VariantFieldName, Builder_.Optional()->Item(Builder_.Variant()->Over(varStructType->Build()).Build()).Build(), &visitedOneofs[oneofDescriptor]);
                        break;
                    }
                }
            }
        } else {
            // Запоминаем поле по соответствующему тегу из proto
            auto& field = message->Fields[fd->number()];
            makeField(field.Pos, field.Flags, structType, fd, fieldContext);
        }
    }

    // Завершаем создание текушей структуры
    message->StructType = structType->Build();

    auto typeHelper = Builder_.TypeInfoHelper();
    auto structTypeInspector = TStructTypeInspector(*typeHelper, message->StructType);
    if (!structTypeInspector) {
        ythrow yexception() << "invalid struct type of " << fullName << " descriptor";
    }
    message->FieldsCount = structTypeInspector.GetMembersCount();

    // Позиции становятся известны после вызова Build()
    for (auto [oneofDescriptor, pos]: visitedOneofs) {
        if (pos != Max<ui32>()) {
            for (int i = 0; i < oneofDescriptor->field_count(); ++i) {
                message->Fields[oneofDescriptor->field(i)->number()].Pos = pos;
            }
        }
    }

    // Зарегистрируем созданное сообщение в дескрипторе типа.
    Info_->Messages.insert(
        std::make_pair(descriptor->full_name(), message)
    );

    return message->StructType;
}

TType* TTypeBuilder::GetBytesType() {
    if (BasicTypes_[FieldDescriptor::TYPE_BYTES] == nullptr) {
        BasicTypes_[FieldDescriptor::TYPE_BYTES] = Builder_.SimpleType<char*>();
    }
    return BasicTypes_[FieldDescriptor::TYPE_BYTES];
}

TType* TTypeBuilder::GetInt64Type() {
    if (BasicTypes_[FieldDescriptor::TYPE_INT64] == nullptr) {
        BasicTypes_[FieldDescriptor::TYPE_INT64] = Builder_.SimpleType<i64>();
    }
    return BasicTypes_[FieldDescriptor::TYPE_INT64];
}

TType* TTypeBuilder::GetYsonType() {
    if (YsonType_ == nullptr) {
        YsonType_ = Builder_.SimpleType<TYson>();
    }
    return YsonType_;
}

TType* TTypeBuilder::GetUnderlyingType(const FieldDescriptor* fd, bool defaultYtSerialize) {
    FieldDescriptor::Type type = fd->type();

    // Unify types
    switch (type) {
        case FieldDescriptor::TYPE_SFIXED32:
        case FieldDescriptor::TYPE_SINT32:
            type = FieldDescriptor::TYPE_INT32;
            break;
        case FieldDescriptor::TYPE_SFIXED64:
        case FieldDescriptor::TYPE_SINT64:
            type = FieldDescriptor::TYPE_INT64;
            break;
        case FieldDescriptor::TYPE_FIXED32:
            type = FieldDescriptor::TYPE_UINT32;
            break;
        case FieldDescriptor::TYPE_FIXED64:
            type = FieldDescriptor::TYPE_UINT64;
            break;
        case FieldDescriptor::TYPE_FLOAT:
            if (YtMode_) {
                type = FieldDescriptor::TYPE_DOUBLE;
            }
            break;
        case FieldDescriptor::TYPE_STRING:
            if (StringType_ == EProtoStringYqlType::Bytes) {
                type = FieldDescriptor::TYPE_BYTES;
            }
            break;
        case FieldDescriptor::TYPE_MESSAGE:
            return GenerateTypeInfo(fd->message_type(), defaultYtSerialize);
        default:
            ;
    }

    if (BasicTypes_[type] == nullptr) {
        switch (type) {
            case FieldDescriptor::TYPE_INT32:
                BasicTypes_[type] = Builder_.SimpleType<i32>();
                break;
            case FieldDescriptor::TYPE_INT64:
                BasicTypes_[type] = Builder_.SimpleType<i64>();
                break;
            case FieldDescriptor::TYPE_UINT32:
                BasicTypes_[type] = Builder_.SimpleType<ui32>();
                break;
            case FieldDescriptor::TYPE_UINT64:
                BasicTypes_[type] = Builder_.SimpleType<ui64>();
                break;
            case FieldDescriptor::TYPE_FLOAT:
                BasicTypes_[type] = Builder_.SimpleType<float>();
                break;
            case FieldDescriptor::TYPE_DOUBLE:
                BasicTypes_[type] = Builder_.SimpleType<double>();
                break;
            case FieldDescriptor::TYPE_BOOL:
                BasicTypes_[type] = Builder_.SimpleType<bool>();
                break;
            case FieldDescriptor::TYPE_ENUM:
                switch (EnumFormat_) {
                    case EEnumFormat::Number:
                        BasicTypes_[type] = Builder_.SimpleType<i32>();
                        break;
                    case EEnumFormat::Name:
                    case EEnumFormat::FullName:
                        BasicTypes_[type] = Builder_.SimpleType<char*>();
                        break;
                }
                break;
            case FieldDescriptor::TYPE_STRING:
                BasicTypes_[type] = Builder_.SimpleType<TUtf8>();
                break;
            case FieldDescriptor::TYPE_BYTES:
                BasicTypes_[type] = Builder_.SimpleType<char*>();
                break;
            default:
                ythrow yexception() << "Unsupported protobuf type: " << fd->type_name()
                                    << ", field: " << fd->name() << ", " << int(fd->type());
        }
    }

    return BasicTypes_[type];
}

TType* TTypeBuilder::WrapTypeFromModifiers(TType* type,
                                           const FieldDescriptor* fd,
                                           EFieldContext fieldContext,
                                           const std::optional<NYT::NDetail::TProtobufFieldOptions>& ytOpts,
                                           TFlags<EFieldFlag>& flags,
                                           bool recursiveType) {

    if (fd->is_repeated()) {
        // Преобразуем базовый тип к списку
        return GetListType(type, ytOpts, flags);
    } else if (fieldContext == EFieldContext::InsideOneofYtSeparateFields) {
        // For 'one of as separate fields' always make optional type
        // Otherwise always ignore optionality.
        return GetOptionalType(type);
    } else if (fieldContext == EFieldContext::InsideOneofYtVariant) {
        // Do not wrap type for 'one of as variant'.
    } else if (fd->is_optional() && (fd->cpp_type() == FieldDescriptor::CPPTYPE_MESSAGE || !AvoidOptionalScalars(SyntaxAware_, fd))) {
        if (!recursiveType || Recursion_ == ERecursionTraits::BytesV2) {
            return GetOptionalType(type);
        }
        // Needs for backward compatability.
        // We cannot just wrap optional to not break old queries.
        flags |= EFieldFlag::RecursiveOptionalUnwrapped;
        return type;
    } else if (fd->is_required()) {
        // Do not wrap required type.
    }
    return type;
}

TType* TTypeBuilder::GetOptionalType(TType* type) {
    auto ti = Optionals_.find(type);
    if (ti != Optionals_.end()) {
        return ti->second;
    } else {
        auto optionalType = Builder_.Optional()->Item(type).Build();
        Optionals_.insert(std::make_pair(type, optionalType));
        return optionalType;
    }
}

TType* TTypeBuilder::GetListType(TType* type, const std::optional<NYT::NDetail::TProtobufFieldOptions>& ytOpts, TFlags<EFieldFlag>& flags) {
    auto ti = Lists_.find(type);
    if (ti != Lists_.end()) {
        type = ti->second;
    } else {
        auto listType = Builder_.List()->Item(type).Build();
        Lists_.insert(std::make_pair(type, listType));
        type = listType;
    }

    if (OptionalLists_ || (ytOpts && NYT::NDetail::EProtobufListMode::Optional == ytOpts->ListMode)) {
        flags |= EFieldFlag::OptionalContainer;
        type = GetOptionalType(type);
    }
    return type;
}

} // namespace

EFieldContext GetFieldContext(const NProtoBuf::FieldDescriptor* fieldDescriptor, bool ytMode) {
    Y_ABORT_UNLESS(fieldDescriptor);

    // Check if the field is part of a map (key or value)
    if (fieldDescriptor->containing_type()->map_key() == fieldDescriptor) {
        return EFieldContext::MapKey;
    } else if (fieldDescriptor->containing_type()->map_value() == fieldDescriptor) {
        return EFieldContext::MapValue;
    }

    // Check if the field is part of a oneof
    if (fieldDescriptor->containing_oneof() != nullptr) {
        if (!ytMode) {
            return EFieldContext::InsideOneofProtobufSeparateFields;
        }
        switch (NYT::NDetail::GetOneofOptions(fieldDescriptor->containing_oneof()).Mode) {
            case NYT::NDetail::EProtobufOneofMode::SeparateFields:
                return EFieldContext::InsideOneofYtSeparateFields;
            case NYT::NDetail::EProtobufOneofMode::Variant:
                return EFieldContext::InsideOneofYtVariant;
        }
    }

    // Determine if the field is optional, required, or repeated
    if (fieldDescriptor->is_optional()) {
        return EFieldContext::SeparateOptional;
    } else if (fieldDescriptor->is_required()) {
        return EFieldContext::SeparateRequired;
    } else if (fieldDescriptor->is_repeated()) {
        return EFieldContext::SeparateRepeated;
    }

    Y_ABORT("Unhandled field type case");
}

void ProtoTypeBuild(const NProtoBuf::Descriptor* descriptor,
                    const EEnumFormat enumFormat,
                    const ERecursionTraits recursion,
                    const bool optionalLists,
                    IFunctionTypeInfoBuilder& builder,
                    TProtoInfo* info,
                    EProtoStringYqlType stringType,
                    const bool syntaxAware,
                    const bool useJsonName,
                    const bool ytMode)
{
    TTypeBuilder(enumFormat, recursion, ytMode, optionalLists, syntaxAware, useJsonName,
                 stringType, builder).Build(descriptor, info);
}

bool AvoidOptionalScalars(bool syntaxAware, const FieldDescriptor* fd) {
    return syntaxAware && fd->file()->syntax() == FileDescriptor::SYNTAX_PROTO3;
}

} // namespace NUdf
} // namespace NYql
