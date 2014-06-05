$doxydocs=
{
  classes => [
    {
      name => '_Eet_Memfile_Write_Info',
      all_members => [
        {
          name => 'data',
          virtualness => 'non_virtual',
          protection => 'public',
          scope => '_Eet_Memfile_Write_Info'
        },
        {
          name => 'f',
          virtualness => 'non_virtual',
          protection => 'public',
          scope => '_Eet_Memfile_Write_Info'
        },
        {
          name => 'size',
          virtualness => 'non_virtual',
          protection => 'public',
          scope => '_Eet_Memfile_Write_Info'
        }
      ],
      public_members => {
        members => [
          {
            kind => 'variable',
            name => 'f',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'FILE *'
          },
          {
            kind => 'variable',
            name => 'data',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'void **'
          },
          {
            kind => 'variable',
            name => 'size',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'size_t *'
          }
        ]
      },
      brief => {},
      detailed => {}
    }
  ],
  namespaces => [
  ],
  files => [
    {
      name => 'epeg.c',
      includes => [
        {
          name => 'stdio.h'
        },
        {
          name => 'stdlib.h'
        },
        {
          name => 'Epeg.h'
        },
        {
          name => 'epeg_private.h'
        }
      ],
      included_by => [
      ],
      defines => {
        members => [
          {
            kind => 'define',
            name => 'MIN',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            parameters => [
              {
                name => '__x'
              },
              {
                name => '__y'
              }
            ],
            initializer => '((__x) < (__y) ? (__x) : (__y))'
          },
          {
            kind => 'define',
            name => 'MAX',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            parameters => [
              {
                name => '__x'
              },
              {
                name => '__y'
              }
            ],
            initializer => '((__x) > (__y) ? (__x) : (__y))'
          }
        ]
      },
      typedefs => {
        members => [
          {
            kind => 'typedef',
            name => 'Eet_Memfile_Write_Info',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'struct _Eet_Memfile_Write_Info'
          }
        ]
      },
      functions => {
        members => [
          {
            kind => 'function',
            name => '_epeg_open_header',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static Epeg_Image *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_decode',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_scale',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_decode_for_trim',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_trim',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_encode',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_fatal_error_handler',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'cinfo',
                type => 'j_common_ptr'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_file_open',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Open a JPEG image by filename. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'file'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'The file path to open. '
                      }
                    ]
                  }
                ],
                {
                  return => [
                    {
                      type => 'text',
                      content => 'A handle to the opened JPEG file, with the header decoded.'
                    }
                  ]
                },
                {
                  type => 'text',
                  content => 'This function opens the file indicated by the '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'file'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' parameter, and attempts to decode it as a jpeg file. If this failes, NULL is returned. Otherwise a valid handle to an open JPEG file is returned that can be used by other Epeg calls.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'The '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'file'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' must be a pointer to a valid C string, NUL (0 byte) terminated thats is a relative or absolute file path. If not results are not determined.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'See also: '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a9df9756f2e34e67701e7d88bcb43fd16',
                  content => 'epeg_memory_open()'
                },
                {
                  type => 'text',
                  content => ', '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a8faf0f0fab47ac97b86ee7e00e1bee7c',
                  content => 'epeg_close()'
                },
                {
                  type => 'text',
                  content => ' '
                }
              ]
            },
            type => 'Epeg_Image *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'file',
                type => 'const char *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_memory_open',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Open a JPEG image stored in memory. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'data'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A pointer to the memory containing the JPEG data. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'size'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The size of the memory segment containing the JPEG. '
                      }
                    ]
                  }
                ],
                {
                  return => [
                    {
                      type => 'text',
                      content => 'A handle to the opened JPEG, with the header decoded.'
                    }
                  ]
                },
                {
                  type => 'text',
                  content => 'This function opens a JPEG file that is stored in memory pointed to by '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'data'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ', and that is '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'size'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' bytes in size. If successful a valid handle is returned, or on failure NULL is returned.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'See also: '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1ac046eceb33af487e670a6b18d15ccae7',
                  content => 'epeg_file_open()'
                },
                {
                  type => 'text',
                  content => ', '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a8faf0f0fab47ac97b86ee7e00e1bee7c',
                  content => 'epeg_close()'
                },
                {
                  type => 'text',
                  content => ' '
                }
              ]
            },
            type => 'Epeg_Image *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'data',
                type => 'unsigned char *'
              },
              {
                declaration_name => 'size',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_size_get',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Return the original JPEG pixel size. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'w'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A pointer to the width value in pixels to be filled in. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'h'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A pointer to the height value in pixels to be filled in.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'Returns the image size in pixels. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'w',
                type => 'int *'
              },
              {
                declaration_name => 'h',
                type => 'int *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_colorspace_get',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Return the original JPEG pixel color space. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'space'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A pointer to the color space value to be filled in.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'Returns the image color space. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'space',
                type => 'int *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_decode_size_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the size of the image to decode in pixels. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'w'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The width of the image to decode at, in pixels. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'h'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The height of the image to decode at, in pixels.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'Sets the size at which to deocode the JPEG image, giving an optimised load that only decodes the pixels needed. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'w',
                type => 'int'
              },
              {
                declaration_name => 'h',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_decode_bounds_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'x',
                type => 'int'
              },
              {
                declaration_name => 'y',
                type => 'int'
              },
              {
                declaration_name => 'w',
                type => 'int'
              },
              {
                declaration_name => 'h',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_decode_colorspace_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the colorspace in which to decode the image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'colorspace'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The colorspace to decode the image in.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This sets the colorspace to decode the image in. The default is EPEG_YUV8, as this is normally the native colorspace of a JPEG file, avoiding any colorspace conversions for a faster load and/or save. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'colorspace',
                type => 'Epeg_Colorspace'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_pixels_get',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Get a segment of decoded pixels from an image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'x'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle X. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'y'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle Y. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'w'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle width. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'h'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle height. '
                      }
                    ]
                  }
                ],
                {
                  return => [
                    {
                      type => 'text',
                      content => 'Pointer to the top left of the requested pixel block.'
                    }
                  ]
                },
                {
                  type => 'text',
                  content => 'Return image pixels in the decoded format from the specified location rectangle bounded with the box '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'x'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ', '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'y'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'w'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' X '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'y'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => '. The pixel block is packed with no row padding, and it organsied from top-left to bottom right, row by row. You must free the pixel block using '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1adf9efc5d877afebda99aba8d5c2bbb0f',
                  content => 'epeg_pixels_free()'
                },
                {
                  type => 'text',
                  content => ' before you close the image handle, and assume the pixels to be read-only memory.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'On success the pointer is returned, on failure, NULL is returned. Failure may be because the rectangle is out of the bounds of the image, memory allocations failed or the image data cannot be decoded. '
                }
              ]
            },
            type => 'const void *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'x',
                type => 'int'
              },
              {
                declaration_name => 'y',
                type => 'int'
              },
              {
                declaration_name => 'w',
                type => 'int'
              },
              {
                declaration_name => 'h',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_pixels_get_as_RGB8',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Get a segment of decoded pixels from an image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'x'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle X. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'y'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle Y. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'w'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle width. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'h'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Rectangle height. '
                      }
                    ]
                  }
                ],
                {
                  return => [
                    {
                      type => 'text',
                      content => 'Pointer to the top left of the requested pixel block.'
                    }
                  ]
                },
                {
                  type => 'text',
                  content => 'Return image pixels in the decoded format from the specified location rectangle bounded with the box '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'x'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ', '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'y'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'w'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' X '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'y'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => '. The pixel block is packed with no row padding, and it organsied from top-left to bottom right, row by row. You must free the pixel block using '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1adf9efc5d877afebda99aba8d5c2bbb0f',
                  content => 'epeg_pixels_free()'
                },
                {
                  type => 'text',
                  content => ' before you close the image handle, and assume the pixels to be read-only memory.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'On success the pointer is returned, on failure, NULL is returned. Failure may be because the rectangle is out of the bounds of the image, memory allocations failed or the image data cannot be decoded. '
                }
              ]
            },
            type => 'const void *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'x',
                type => 'int'
              },
              {
                declaration_name => 'y',
                type => 'int'
              },
              {
                declaration_name => 'w',
                type => 'int'
              },
              {
                declaration_name => 'h',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_pixels_free',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Free requested pixel block from an image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'data'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The pointer to the image pixels.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This frees the data for a block of pixels requested from image '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'im'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => '. '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'data'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' must be a valid (non NULL) pointer to a pixel block taken from the image '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'im'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1aa3e359181a6a48bd84708828364bf094',
                  content => 'epeg_pixels_get()'
                },
                {
                  type => 'text',
                  content => ' and mustbe called before the image is closed by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a8faf0f0fab47ac97b86ee7e00e1bee7c',
                  content => 'epeg_close()'
                },
                {
                  type => 'text',
                  content => '. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'data',
                type => 'const void *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_comment_get',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Get the image comment field as a string. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  }
                ],
                {
                  return => [
                    {
                      type => 'text',
                      content => 'A pointer to the loaded image comments.'
                    }
                  ]
                },
                {
                  type => 'text',
                  content => 'This function returns the comment field as a string (NUL byte terminated) of the loaded image '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'im'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ', if there is a comment, or NULL if no comment is saved with the image. Consider the string returned to be read-only. '
                }
              ]
            },
            type => 'const char *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_thumbnail_comments_get',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Get thumbnail comments of loaded image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'info'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'Pointer to a thumbnail info struct to be filled in.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This function retrieves thumbnail comments written by Epeg to any saved JPEG files. If no thumbnail comments were saved, the fields will be 0 in the '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'info'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' struct on return. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'info',
                type => 'Epeg_Thumbnail_Info *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_comment_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the comment field of the image for saving. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'comment'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The comment to set.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'Set the comment for the image file for when it gets saved. This is a NUL byte terminated C string. If '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'comment'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' is NULL the output file will have no comment field.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'The default comment will be any comment loaded from the input file. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'comment',
                type => 'const char *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_quality_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the encoding quality of the saved image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'quality'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The quality of encoding from 0 to 100.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'Set the quality of the output encoded image. Values from 0 to 100 inclusive are valid, with 100 being the maximum quality, and 0 being the minimum. If the quality is set equal to or above 90%, the output U and V color planes are encoded at 1:1 with the Y plane.'
                },
                {
                  type => 'parbreak'
                },
                {
                  type => 'text',
                  content => 'The default quality is 75. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'quality',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_thumbnail_comments_enable',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Enable thumbnail comments in saved image. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'onoff'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A boolean on and off enabling flag.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'if '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'onoff'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' is 1, the output file will have thumbnail comments added to it, and if it is 0, it will not. The default is 0. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'onoff',
                type => 'int'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_file_output_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the output file path for the image when saved. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'file'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'The path to the output file.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This sets the output file path name (either a full or relative path name) to where the file will be written when saved. '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'file'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' must be a NUL terminated C string conatining the path to the file to be saved to. If it is NULL, the image will not be saved to a file when calling '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a12a018084510ebdc0e627f56305fea79',
                  content => 'epeg_encode()'
                },
                {
                  type => 'text',
                  content => '. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'file',
                type => 'const char *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_memory_output_set',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Set the output file to be a block of allocated memory. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'data'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A pointer to a pointer to a memory block. '
                      }
                    ]
                  },
                  {
                    parameters => [
                      {
                        name => 'size'
                      }
                    ],
                    doc => [
                      {
                        type => 'parbreak'
                      },
                      {
                        type => 'text',
                        content => 'A pointer to a counter of the size of the memory block.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This sets the output encoding of the image when saved to be allocated memory. After '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a8faf0f0fab47ac97b86ee7e00e1bee7c',
                  content => 'epeg_close()'
                },
                {
                  type => 'text',
                  content => ' is called the pointer pointed to by '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'data'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' and the integer pointed to by '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'size'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' will contain the pointer to the memory block and its size in bytes, respecitvely. The memory block can be freed with the free() function call. If the save fails the pointer to the memory block will be unaffected, as will the size. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              },
              {
                declaration_name => 'data',
                type => 'unsigned char **'
              },
              {
                declaration_name => 'size',
                type => 'int *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_encode',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'This saves the image to its specified destination. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This saves the image '
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'yes'
                },
                {
                  type => 'text',
                  content => 'im'
                },
                {
                  type => 'style',
                  style => 'code',
                  enable => 'no'
                },
                {
                  type => 'text',
                  content => ' to its destination specified by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a4aa4c7bbf3edf1f24603d3b4dad684b4',
                  content => 'epeg_file_output_set()'
                },
                {
                  type => 'text',
                  content => ' or '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1ae0e91c160074e6d96b7e366fb0eb6ec8',
                  content => 'epeg_memory_output_set()'
                },
                {
                  type => 'text',
                  content => '. The image will be encoded at the deoded pixel size, using the quality, comment and thumbnail comment settings set on the image. '
                }
              ]
            },
            type => 'int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_trim',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'FIXME: Document this. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'FIXME: Document this. '
                }
              ]
            },
            type => 'int',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => 'epeg_close',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {
              doc => [
                {
                  type => 'text',
                  content => 'Close an image handle. '
                }
              ]
            },
            detailed => {
              doc => [
                params => [
                  {
                    parameters => [
                      {
                        name => 'im'
                      }
                    ],
                    doc => [
                      {
                        type => 'text',
                        content => 'A handle to an opened Epeg image.'
                      }
                    ]
                  }
                ],
                {
                  type => 'text',
                  content => 'This closes an opened image handle and frees all memory associated with it. It does not free encoded data generated by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1ae0e91c160074e6d96b7e366fb0eb6ec8',
                  content => 'epeg_memory_output_set()'
                },
                {
                  type => 'text',
                  content => ' followed by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1a12a018084510ebdc0e627f56305fea79',
                  content => 'epeg_encode()'
                },
                {
                  type => 'text',
                  content => ' nor does it guarantee to free any data recieved by '
                },
                {
                  type => 'url',
                  link => 'epeg_8c_1aa3e359181a6a48bd84708828364bf094',
                  content => 'epeg_pixels_get()'
                },
                {
                  type => 'text',
                  content => '. Once an image handle is closed consider it invalid. '
                }
              ]
            },
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'im',
                type => 'Epeg_Image *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_memfile_read_open',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'FILE *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'data',
                type => 'void *'
              },
              {
                declaration_name => 'size',
                type => 'size_t'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_memfile_read_close',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'f',
                type => 'FILE *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_memfile_write_open',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'FILE *',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'data',
                type => 'void **'
              },
              {
                declaration_name => 'size',
                type => 'size_t *'
              }
            ]
          },
          {
            kind => 'function',
            name => '_epeg_memfile_write_close',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'no',
            brief => {},
            detailed => {},
            type => 'void',
            const => 'no',
            volatile => 'no',
            parameters => [
              {
                declaration_name => 'f',
                type => 'FILE *'
              }
            ]
          }
        ]
      },
      variables => {
        members => [
          {
            kind => 'variable',
            name => '_epeg_memfile_info_alloc_num',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            initializer => '= 0'
          },
          {
            kind => 'variable',
            name => '_epeg_memfile_info_num',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static int',
            initializer => '= 0'
          },
          {
            kind => 'variable',
            name => '_epeg_memfile_info',
            virtualness => 'non_virtual',
            protection => 'public',
            static => 'yes',
            brief => {},
            detailed => {},
            type => 'static Eet_Memfile_Write_Info *',
            initializer => '= NULL'
          }
        ]
      },
      brief => {
        doc => [
          {
            type => 'text',
            content => 'Epeg JPEG Thumbnailer library. '
          }
        ]
      },
      detailed => {
        doc => [
          {
            type => 'text',
            content => 'These routines are used for the Epeg library. '
          }
        ]
      }
    }
  ],
  groups => [
  ],
  pages => [
    {
      name => 'todo',
      title4 => 'Todo List',
      detailed => {
        doc => [
          {
            type => 'anchor',
            id => 'todo_1_todo000001'
          },
          {
            type => 'text',
            content => 'page '
          },
          {
            type => 'ref',
            link => 'index',
            content => [
              {
                type => 'text',
                content => 'Epeg Library Documentation'
              }
            ]
          },
          {
            type => 'text',
            content => ' Check all input parameters for sanity. '
          },
          {
            type => 'parbreak'
          },
          {
            type => 'text',
            content => 'Actually report error conditions properly.'
          }
        ]
      }
    },
    {
      name => 'index',
      title4 => 'Epeg Library Documentation',
      detailed => {
        doc => [
          {
            type => 'text',
            content => ' '
          },
          {
            version => [
              {
                type => 'text',
                content => '0.9.0 '
              }
            ]
          },
          {
            author => [
              {
                type => 'text',
                content => 'Carsten Haitzler '
              },
              {
                type => 'url',
                content => 'raster@rasterman.com'
              },
              {
                type => 'text',
                content => ' '
              }
            ]
          },
          {
            date => [
              {
                type => 'text',
                content => '2000-2003'
              }
            ]
          },
          {
            type => 'sect1',
            content => [
              {
                type => 'text',
                content => 'An IMMENSELY FAST JPEG thumbnailer library API.'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'Why write this? It\'s a convenience library API to using libjpeg to load JPEG images destined to be turned into thumbnails of the original, saving information with these thumbnails, retreiving it and managing to load the image ready for scaling with the minimum of fuss and CPU overhead.'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'This means it\'s insanely fast at loading large JPEG images and scaling them down to tiny thumbnails. It\'s speedup will be proportional to the size difference between the source image and the output thumbnail size as a count of their pixels.'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'It makes use of libjpeg features of being able to load an image by only decoding the DCT coefficients needed to reconstruct an image of the size desired. This gives a massive speedup. If you do not try and access the pixels in a format other than YUV (or GRAY8 if the source is grascale) then it also avoids colorspace conversions as well.'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'Using the library is very easy, look at this example:'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'You can compile this program with as small a line as:'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'preformatted',
                content => 'gcc epeg_test.c -o epeg_test `epeg-config --cflags --libs`
'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'text',
                content => 'It is a very simple library that just makes life easier when tyring to generate lots of thumbnails for large JPEG images as quickly as possible. Your milage may vary, but it should save you lots of time and effort in using libjpeg in general.'
              },
              {
                type => 'parbreak'
              },
              {
                type => 'xrefitem',
                content => [
                  {
                    type => 'text',
                    content => 'Check all input parameters for sanity. '
                  },
                  {
                    type => 'parbreak'
                  },
                  {
                    type => 'text',
                    content => 'Actually report error conditions properly.'
                  }
                ]
              }
            ]
          }
        ]
      }
    }
  ]
};
1;
