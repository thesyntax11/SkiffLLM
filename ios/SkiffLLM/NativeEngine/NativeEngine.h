#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface LlamaGenerationResult : NSObject
@property (nonatomic, copy) NSString *text;
@property (nonatomic) int32_t promptTokens;
@property (nonatomic) int32_t generatedTokens;
@property (nonatomic) double promptMs;
@property (nonatomic) double generationMs;
@property (nonatomic) double tokensPerSecond;
@property (nonatomic) BOOL stopped;
@end

@interface NativeEngine : NSObject

- (int)contextCapacity;
- (NSString *)activeBackends;
- (NSString *)modelDescription;

- (BOOL)warmup:(NSError **)error NS_SWIFT_NOTHROW;

- (BOOL)loadModelAtPath:(NSString *)path
            contextSize:(int)contextSize
               threads:(int)threads
             gpuLayers:(int)gpuLayers
           chatTemplate:(NSString *)chatTemplate
                 error:(NSError **)error NS_SWIFT_NOTHROW;

- (void)generateMessages:(NSArray<NSDictionary<NSString *, NSString *> *> *)messages
              temperature:(float)temperature
                    topP:(float)topP
                    topK:(int)topK
                    minP:(float)minP
                 typicalP:(float)typicalP
           repeatPenalty:(float)repeatPenalty
           repeatLastN:(int)repeatLastN
              maxTokens:(int)maxTokens
                    seed:(uint32_t)seed
           stopSequences:(NSArray<NSString *> *)stopSequences
           tokenCallback:(void (^_Nullable)(NSString *_Nullable token))tokenCallback
             completion:(void (^_Nullable)(LlamaGenerationResult *_Nullable result,
                                           NSError *_Nullable error))completion;

- (NSString *)skillCatalogJSON;
- (NSString *)executeSkill:(NSString *)name
                  argsJSON:(NSString *)argsJSON
                     error:(NSError **)error NS_SWIFT_NOTHROW;
- (NSString *)memoryLoad;
- (BOOL)memoryAppend:(NSString *)text error:(NSError **)error NS_SWIFT_NOTHROW;
- (BOOL)memoryClear:(NSError **)error NS_SWIFT_NOTHROW;
- (NSString *)usageStatsJSON;

- (void)stop;
- (void)close;

@end

NS_ASSUME_NONNULL_END
